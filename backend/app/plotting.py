import io
import math
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone

import matplotlib.dates as mdates
import numpy as np
from matplotlib.figure import Figure

from app.handlers import InvalidRequest
from app.repository import StoredReading

WIDTH_IN = 10
HEIGHT_IN = 6.5
DPI = 100

TARGET_POINTS = 150
NICE_BUCKETS_S = [10, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 21600, 86400]
UNIT_SECONDS = {"s": 1, "m": 60, "h": 3600, "d": 86400}
BAND_PERCENTILES = (10, 90)
MAX_GAP_BUCKETS = 3  # a longer silence is drawn as a gap, not as a line across it

AUTO = "auto"
RAW = "raw"


@dataclass(frozen=True)
class Bucket:
    start: datetime
    devices_mean: float
    devices_low: float
    devices_high: float
    above_mean: float


def parse_bucket(text: str) -> str | int:
    """'auto', 'raw' or a size such as 30s, 5m, 1h; returns the size in seconds for the last."""
    if text in (AUTO, RAW):
        return text
    number, unit = text[:-1], text[-1:]
    if not number.isdigit() or unit not in UNIT_SECONDS or int(number) == 0:
        raise InvalidRequest("bucket must be auto, raw, or look like 30s, 5m or 1h")
    return int(number) * UNIT_SECONDS[unit]


def auto_bucket_seconds(readings: list[StoredReading]) -> int:
    span = (readings[-1].ts - readings[0].ts).total_seconds()
    wanted = span / TARGET_POINTS
    return next((step for step in NICE_BUCKETS_S if step >= wanted), NICE_BUCKETS_S[-1])


def bucketize(readings: list[StoredReading], seconds: int) -> list[Bucket]:
    """Readings of one device, oldest first, averaged into fixed time buckets."""
    groups: dict[int, list[StoredReading]] = defaultdict(list)
    for reading in readings:
        groups[int(reading.ts.timestamp()) // seconds].append(reading)

    return [_summarize(index * seconds, rows) for index, rows in sorted(groups.items())]


def _summarize(start_s: int, rows: list[StoredReading]) -> Bucket:
    devices = [row.devices for row in rows]
    low, high = np.percentile(devices, BAND_PERCENTILES)
    return Bucket(
        start=datetime.fromtimestamp(start_s, tz=timezone.utc),
        devices_mean=sum(devices) / len(rows),
        devices_low=float(low),
        devices_high=float(high),
        above_mean=sum(row.devices_above_rssi for row in rows) / len(rows),
    )


def describe_bucket(seconds: int | None) -> str:
    if seconds is None:
        return "every scan window"
    if seconds % 3600 == 0:
        return f"{seconds // 3600} h averages"
    if seconds % 60 == 0:
        return f"{seconds // 60} min averages"
    return f"{seconds} s averages"


def render_devices_plot(readings: list[StoredReading], bucket: str | int = AUTO) -> bytes:
    figure = Figure(figsize=(WIDTH_IN, HEIGHT_IN), dpi=DPI, layout="constrained")

    if not readings:
        axes = figure.subplots()
        axes.text(0.5, 0.5, "No readings in this range", ha="center", va="center")
        axes.set_axis_off()
    else:
        _draw(figure, readings, bucket)

    buffer = io.BytesIO()
    figure.savefig(buffer, format="png")
    return buffer.getvalue()


def _draw(figure: Figure, readings: list[StoredReading], bucket: str | int) -> None:
    seconds = auto_bucket_seconds(readings) if bucket == AUTO else bucket
    seconds = None if seconds == RAW else seconds

    by_device: dict[str, list[StoredReading]] = defaultdict(list)
    for reading in readings:
        by_device[reading.device_id].append(reading)

    top, bottom = figure.subplots(
        2, 1, sharex=True, height_ratios=[3, 2]
    )
    for index, (device_id, rows) in enumerate(sorted(by_device.items())):
        color = f"C{index % 10}"
        if seconds is None:
            times = [row.ts for row in rows]
            top.plot(times, [row.devices for row in rows], color=color, label=device_id)
            bottom.plot(times, [row.devices_above_rssi for row in rows], color=color)
            continue

        buckets = bucketize(rows, seconds)
        times, mean, low, high, above = _with_gaps(buckets, seconds)
        top.fill_between(times, low, high, color=color, alpha=0.2, linewidth=0)
        top.plot(times, mean, color=color, linewidth=1.6, label=device_id)
        bottom.plot(times, above, color=color, linewidth=1.6)

    locator = mdates.AutoDateLocator()
    bottom.xaxis.set_major_locator(locator)
    bottom.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    bottom.set_xlabel("Time (UTC)")

    top.set_ylabel("Devices per window")
    bottom.set_ylabel("Above threshold")
    for axes in (top, bottom):
        axes.set_ylim(bottom=0)
        axes.grid(alpha=0.3)

    subtitle = describe_bucket(seconds)
    if seconds is not None:
        subtitle += ", shaded band is the 10th to 90th percentile"
    top.set_title(subtitle, fontsize=10, loc="left")
    figure.legend(*top.get_legend_handles_labels(), loc="outside lower center", ncols=4)


def _with_gaps(buckets: list[Bucket], seconds: int):
    times, mean, low, high, above = [], [], [], [], []
    previous = None
    for bucket in buckets:
        if previous is not None:
            missing = (bucket.start - previous).total_seconds() / seconds
            if missing > MAX_GAP_BUCKETS:
                gap = previous + (bucket.start - previous) / 2
                times.append(gap)
                mean.append(math.nan)
                low.append(math.nan)
                high.append(math.nan)
                above.append(math.nan)
        times.append(bucket.start)
        mean.append(bucket.devices_mean)
        low.append(bucket.devices_low)
        high.append(bucket.devices_high)
        above.append(bucket.above_mean)
        previous = bucket.start
    return times, mean, low, high, above
