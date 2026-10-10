from datetime import datetime, timedelta, timezone

from app.plotting import render_devices_plot
from app.repository import StoredReading

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
START = datetime(2026, 10, 9, 12, 0, tzinfo=timezone.utc)


def stored(device_id: str, minutes: int, devices: int, above: int) -> StoredReading:
    ts = START + timedelta(minutes=minutes)
    return StoredReading(
        device_id=device_id,
        ts=ts,
        received_at=ts,
        scan_window_s=10,
        rssi_min=-80,
        devices=devices,
        devices_above_rssi=above,
        avg_rssi=-80,
        temp_c=None,
        humidity_pct=None,
        lux=None,
    )


def test_readings_render_as_a_png():
    readings = [stored("guzvo-a", m, 10 + m, 3 + m) for m in range(0, 30, 5)]
    assert render_devices_plot(readings).startswith(PNG_SIGNATURE)


def test_an_empty_range_still_renders_an_image():
    assert render_devices_plot([]).startswith(PNG_SIGNATURE)


def test_several_devices_and_a_single_reading_render():
    readings = [stored("guzvo-a", 0, 5, 2), stored("guzvo-b", 0, 8, 4)]
    assert render_devices_plot(readings).startswith(PNG_SIGNATURE)
