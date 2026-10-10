import io
from collections import defaultdict

import matplotlib.dates as mdates
from matplotlib.figure import Figure

from app.repository import StoredReading

WIDTH_IN = 10
HEIGHT_IN = 4.5
DPI = 100


def render_devices_plot(readings: list[StoredReading]) -> bytes:
    """One colour per device: solid line for all devices, dashed for those above the RSSI threshold."""
    figure = Figure(figsize=(WIDTH_IN, HEIGHT_IN), dpi=DPI, layout="constrained")
    axes = figure.subplots()

    if not readings:
        axes.text(0.5, 0.5, "No readings in this range", ha="center", va="center")
        axes.set_axis_off()
    else:
        _draw_series(axes, readings)

    buffer = io.BytesIO()
    figure.savefig(buffer, format="png")
    return buffer.getvalue()


def _draw_series(axes, readings: list[StoredReading]) -> None:
    by_device: dict[str, list[StoredReading]] = defaultdict(list)
    for reading in readings:
        by_device[reading.device_id].append(reading)

    for index, (device_id, rows) in enumerate(sorted(by_device.items())):
        color = f"C{index % 10}"
        times = [row.ts for row in rows]
        axes.plot(times, [row.devices for row in rows], color=color, label=f"{device_id} all")
        axes.plot(
            times,
            [row.devices_above_rssi for row in rows],
            color=color,
            linestyle="--",
            label=f"{device_id} above threshold",
        )

    locator = mdates.AutoDateLocator()
    axes.xaxis.set_major_locator(locator)
    axes.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    axes.set_ylabel("Unique devices per window")
    axes.set_xlabel("Time (UTC)")
    axes.set_ylim(bottom=0)
    axes.grid(alpha=0.3)
    axes.get_figure().legend(loc="outside lower center", ncols=2)
