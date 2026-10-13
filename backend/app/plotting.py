import io
import math
from collections import defaultdict

import matplotlib.dates as mdates
from matplotlib.figure import Figure

from app.buckets import PlotData
from app.repository import BucketSummary

WIDTH_IN = 10
HEIGHT_IN = 6.5
DPI = 100

MAX_GAP_BUCKETS = 3  # a longer silence is drawn as a gap, not as a line across it


def describe_bucket(seconds: int | None) -> str:
    if seconds is None:
        return "every scan window"
    if seconds % 3600 == 0:
        return f"{seconds // 3600} h averages"
    if seconds % 60 == 0:
        return f"{seconds // 60} min averages"
    return f"{seconds} s averages"


def render_devices_plot(data: PlotData) -> bytes:
    figure = Figure(figsize=(WIDTH_IN, HEIGHT_IN), dpi=DPI, layout="constrained")

    if not data.readings and not data.buckets:
        axes = figure.subplots()
        axes.text(0.5, 0.5, "No readings in this range", ha="center", va="center")
        axes.set_axis_off()
    else:
        _draw(figure, data)

    buffer = io.BytesIO()
    figure.savefig(buffer, format="png")
    return buffer.getvalue()


def _draw(figure: Figure, data: PlotData) -> None:
    seconds = data.bucket_seconds
    top, bottom = figure.subplots(2, 1, sharex=True, height_ratios=[3, 2])

    if seconds is None:
        _draw_raw(top, bottom, data)
    else:
        _draw_buckets(top, bottom, data, seconds)

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


def _draw_raw(top, bottom, data: PlotData) -> None:
    by_device = defaultdict(list)
    for reading in data.readings:
        by_device[reading.device_id].append(reading)

    for index, (device_id, rows) in enumerate(sorted(by_device.items())):
        color = f"C{index % 10}"
        times = [row.ts for row in rows]
        top.plot(times, [row.devices for row in rows], color=color, label=device_id)
        bottom.plot(times, [row.devices_above_rssi for row in rows], color=color)


def _draw_buckets(top, bottom, data: PlotData, seconds: int) -> None:
    by_device = defaultdict(list)
    for bucket in data.buckets:
        by_device[bucket.device_id].append(bucket)

    for index, (device_id, buckets) in enumerate(sorted(by_device.items())):
        color = f"C{index % 10}"
        times, mean, low, high, above = _with_gaps(buckets, seconds)
        top.fill_between(times, low, high, color=color, alpha=0.2, linewidth=0)
        top.plot(times, mean, color=color, linewidth=1.6, label=device_id)
        bottom.plot(times, above, color=color, linewidth=1.6)


def _with_gaps(buckets: list[BucketSummary], seconds: int):
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
