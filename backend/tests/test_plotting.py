from datetime import datetime, timedelta, timezone

import pytest

from app.handlers import InvalidRequest
from app.plotting import (
    auto_bucket_seconds,
    bucketize,
    describe_bucket,
    parse_bucket,
    render_devices_plot,
)
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


def test_bucketize_averages_each_time_bucket():
    readings = [stored("guzvo-a", 0, 10, 2), stored("guzvo-a", 1, 20, 4), stored("guzvo-a", 6, 30, 6)]
    buckets = bucketize(readings, 300)
    assert [b.start for b in buckets] == [START, START + timedelta(minutes=5)]
    assert [b.devices_mean for b in buckets] == [15, 30]
    assert [b.above_mean for b in buckets] == [3, 6]


def test_bucket_band_stays_inside_the_observed_range():
    readings = [stored("guzvo-a", 0, n, 0) for n in (4, 8, 8, 8, 40)]
    (bucket,) = bucketize(readings, 3600)
    assert 4 <= bucket.devices_low <= bucket.devices_mean <= bucket.devices_high <= 40


def test_a_single_reading_has_a_zero_width_band():
    (bucket,) = bucketize([stored("guzvo-a", 0, 12, 3)], 600)
    assert bucket.devices_low == bucket.devices_high == bucket.devices_mean == 12


@pytest.mark.parametrize(
    ("text", "expected"),
    [("auto", "auto"), ("raw", "raw"), ("30s", 30), ("5m", 300), ("2h", 7200), ("1d", 86400)],
)
def test_parse_bucket(text, expected):
    assert parse_bucket(text) == expected


@pytest.mark.parametrize("text", ["0m", "m", "5", "5x", "-5m", "1.5h", ""])
def test_parse_bucket_rejects_bad_sizes(text):
    with pytest.raises(InvalidRequest):
        parse_bucket(text)


def test_auto_bucket_gives_a_readable_number_of_points():
    def span(minutes: int):
        return [stored("guzvo-a", 0, 1, 0), stored("guzvo-a", minutes, 1, 0)]

    assert auto_bucket_seconds(span(10)) == 10
    assert auto_bucket_seconds(span(60)) == 30
    assert auto_bucket_seconds(span(24 * 60)) == 600
    assert auto_bucket_seconds(span(30 * 24 * 60)) == 21600
    assert auto_bucket_seconds(span(365 * 24 * 60)) == 86400  # the largest size


def test_bucket_descriptions():
    assert describe_bucket(None) == "every scan window"
    assert describe_bucket(30) == "30 s averages"
    assert describe_bucket(600) == "10 min averages"
    assert describe_bucket(7200) == "2 h averages"


def test_a_long_silence_and_raw_mode_still_render():
    readings = [stored("guzvo-a", m, 10, 2) for m in (0, 1, 2, 600, 601)]
    assert render_devices_plot(readings).startswith(PNG_SIGNATURE)
    assert render_devices_plot(readings, "raw").startswith(PNG_SIGNATURE)
    assert render_devices_plot(readings, 60).startswith(PNG_SIGNATURE)
