from datetime import datetime, timedelta, timezone

import pytest

from app.buckets import (
    MAX_BUCKETS,
    auto_bucket_seconds,
    load_plot_data,
    parse_bucket,
)
from app.handlers import InvalidRequest
from tests.fakes import InMemoryRepository, repository_with, stored_reading

START = datetime(2026, 10, 9, 12, 0, tzinfo=timezone.utc)
END = START + timedelta(days=400)


def at(minutes: int) -> datetime:
    return START + timedelta(minutes=minutes)


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
    minutes = 60
    assert auto_bucket_seconds(10 * minutes) == 10
    assert auto_bucket_seconds(60 * minutes) == 30
    assert auto_bucket_seconds(24 * 60 * minutes) == 600
    assert auto_bucket_seconds(30 * 24 * 60 * minutes) == 21600
    assert auto_bucket_seconds(365 * 24 * 60 * minutes) == 86400  # the largest size


def test_buckets_average_each_time_bucket():
    repository = repository_with(
        [
            stored_reading("guzvo-a", at(0), 10, 2),
            stored_reading("guzvo-a", at(1), 20, 4),
            stored_reading("guzvo-a", at(6), 30, 6),
        ]
    )
    data = load_plot_data(repository, None, END, None, 100, 300)
    assert data.bucket_seconds == 300
    assert [b.start for b in data.buckets] == [at(0), at(5)]
    assert [b.devices_mean for b in data.buckets] == [15, 30]
    assert [b.above_mean for b in data.buckets] == [3, 6]


def test_the_band_stays_inside_the_observed_range():
    rows = [stored_reading("guzvo-a", at(0) + timedelta(seconds=i), n) for i, n in enumerate((4, 8, 8, 8, 40))]
    (bucket,) = load_plot_data(repository_with(rows), None, END, None, 100, 3600).buckets
    assert 4 <= bucket.devices_low <= bucket.devices_mean <= bucket.devices_high <= 40


def test_the_band_interpolates_like_postgres():
    rows = [stored_reading("guzvo-a", at(0) + timedelta(seconds=i), n) for i, n in enumerate((4, 8, 8, 8, 40))]
    (bucket,) = load_plot_data(repository_with(rows), None, END, None, 100, 3600).buckets
    assert bucket.devices_low == pytest.approx(5.6)
    assert bucket.devices_high == pytest.approx(27.2)


def test_a_single_reading_has_a_zero_width_band():
    repository = repository_with([stored_reading("guzvo-a", at(0), 12, 3)])
    (bucket,) = load_plot_data(repository, None, END, None, 100, 600).buckets
    assert bucket.devices_low == bucket.devices_high == bucket.devices_mean == 12


def test_devices_are_bucketed_separately():
    repository = repository_with(
        [stored_reading("guzvo-a", at(0), 10), stored_reading("guzvo-b", at(0), 30)]
    )
    data = load_plot_data(repository, None, END, None, 100, 600)
    assert [(b.device_id, b.devices_mean) for b in data.buckets] == [
        ("guzvo-a", 10),
        ("guzvo-b", 30),
    ]


def test_auto_picks_the_size_from_the_matching_span():
    rows = [stored_reading("guzvo-a", at(m), 5) for m in (0, 24 * 60)]
    data = load_plot_data(repository_with(rows), None, END, None, 100, "auto")
    assert data.bucket_seconds == 600


def test_the_row_limit_does_not_apply_to_averaged_plots():
    rows = [stored_reading("guzvo-a", at(0) + timedelta(seconds=i), 5) for i in range(50)]
    data = load_plot_data(repository_with(rows), None, END, None, 3, 3600)
    assert [b.devices_mean for b in data.buckets] == [5]


def test_raw_mode_returns_the_newest_rows_up_to_the_limit():
    rows = [stored_reading("guzvo-a", at(m), m) for m in range(10)]
    data = load_plot_data(repository_with(rows), None, END, None, 3, "raw")
    assert data.bucket_seconds is None
    assert [r.devices for r in data.readings] == [7, 8, 9]


def test_an_empty_range_has_no_data():
    data = load_plot_data(InMemoryRepository(), None, END, None, 100, "auto")
    assert data.readings == [] and data.buckets == []


def test_a_bucket_far_too_small_for_the_range_is_rejected():
    rows = [stored_reading("guzvo-a", at(0), 5), stored_reading("guzvo-a", at(60 * 24 * 60), 5)]
    with pytest.raises(InvalidRequest):
        load_plot_data(repository_with(rows), None, END, None, 100, 10)
    assert MAX_BUCKETS >= 1_000


def test_an_inverted_range_is_rejected():
    with pytest.raises(InvalidRequest):
        load_plot_data(InMemoryRepository(), END, START, None, 100, "auto")
