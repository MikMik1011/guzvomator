from datetime import datetime, timedelta, timezone

from app.buckets import PlotData
from app.plotting import describe_bucket, render_devices_plot
from app.repository import BucketSummary
from tests.fakes import stored_reading

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
START = datetime(2026, 10, 9, 12, 0, tzinfo=timezone.utc)


def raw(rows) -> PlotData:
    return PlotData(None, rows, [])


def bucketed(buckets, seconds=300) -> PlotData:
    return PlotData(seconds, [], buckets)


def bucket(device_id: str, minutes: int, mean: float = 10) -> BucketSummary:
    return BucketSummary(
        device_id=device_id,
        start=START + timedelta(minutes=minutes),
        devices_mean=mean,
        devices_low=mean - 2,
        devices_high=mean + 2,
        above_mean=mean / 4,
    )


def test_raw_readings_render_as_a_png():
    rows = [stored_reading("guzvo-a", START + timedelta(minutes=m), 10 + m, 3) for m in range(5)]
    assert render_devices_plot(raw(rows)).startswith(PNG_SIGNATURE)


def test_buckets_render_as_a_png():
    buckets = [bucket("guzvo-a", m) for m in range(0, 30, 5)]
    assert render_devices_plot(bucketed(buckets)).startswith(PNG_SIGNATURE)


def test_an_empty_range_still_renders_an_image():
    assert render_devices_plot(PlotData(None, [], [])).startswith(PNG_SIGNATURE)


def test_several_devices_and_a_single_bucket_render():
    buckets = [bucket("guzvo-a", 0, 5), bucket("guzvo-b", 0, 8)]
    assert render_devices_plot(bucketed(buckets)).startswith(PNG_SIGNATURE)


def test_a_long_silence_between_buckets_still_renders():
    buckets = [bucket("guzvo-a", m) for m in (0, 5, 10, 600, 605)]
    assert render_devices_plot(bucketed(buckets)).startswith(PNG_SIGNATURE)


def test_bucket_descriptions():
    assert describe_bucket(None) == "every scan window"
    assert describe_bucket(30) == "30 s averages"
    assert describe_bucket(600) == "10 min averages"
    assert describe_bucket(7200) == "2 h averages"


def test_importing_the_api_does_not_load_the_plotting_libraries():
    import subprocess
    import sys

    code = "import app.api, sys; sys.exit('matplotlib' in sys.modules or 'numpy' in sys.modules)"
    assert subprocess.run([sys.executable, "-c", code]).returncode == 0
