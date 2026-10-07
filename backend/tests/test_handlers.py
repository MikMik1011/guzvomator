from datetime import datetime, timedelta, timezone

import pytest

from app.handlers import (
    MAX_FUTURE_S,
    InvalidRequest,
    StoreOutcome,
    list_readings,
    parse_period,
    readings_to_csv,
    readings_to_dicts,
    resolve_range,
    store_reading,
)
from app.models import Reading
from tests.fakes import InMemoryRepository
from tests.payloads import NOW_TS, valid_payload

NOW = datetime.fromtimestamp(NOW_TS, tz=timezone.utc)


def reading(**overrides) -> Reading:
    return Reading(**valid_payload(**overrides))


def test_new_reading_is_created():
    repository = InMemoryRepository()
    assert store_reading(repository, reading(), NOW) is StoreOutcome.CREATED
    assert len(repository.rows) == 1


def test_same_device_and_ts_is_a_duplicate():
    repository = InMemoryRepository()
    store_reading(repository, reading(), NOW)
    assert store_reading(repository, reading(devices=99), NOW) is StoreOutcome.DUPLICATE
    assert len(repository.rows) == 1


def test_same_ts_from_another_device_is_not_a_duplicate():
    repository = InMemoryRepository()
    store_reading(repository, reading(), NOW)
    assert store_reading(repository, reading(device_id="other"), NOW) is StoreOutcome.CREATED


def test_reading_from_the_future_is_rejected():
    with pytest.raises(InvalidRequest):
        store_reading(InMemoryRepository(), reading(ts=NOW_TS + 3600), NOW)


def test_small_clock_skew_is_tolerated():
    outcome = store_reading(InMemoryRepository(), reading(ts=NOW_TS + 60), NOW)
    assert outcome is StoreOutcome.CREATED


def stored_repository() -> InMemoryRepository:
    repository = InMemoryRepository()
    for offset in (0, 60, 120):
        store_reading(repository, reading(ts=NOW_TS + offset), NOW + timedelta(seconds=offset))
    store_reading(repository, reading(device_id="other", ts=NOW_TS + 30), NOW)
    return repository


def test_list_filters_by_time_range_oldest_first():
    rows = list_readings(stored_repository(), NOW, NOW + timedelta(seconds=100))
    assert [r.ts.timestamp() - NOW_TS for r in rows] == [0, 30, 60]


def test_list_filters_by_device():
    rows = list_readings(stored_repository(), NOW, NOW + timedelta(hours=1), "other")
    assert [r.device_id for r in rows] == ["other"]


def test_list_respects_limit():
    rows = list_readings(stored_repository(), NOW, NOW + timedelta(hours=1), limit=2)
    assert len(rows) == 2


def test_list_treats_naive_datetimes_as_utc():
    naive_start = NOW.replace(tzinfo=None)
    rows = list_readings(stored_repository(), naive_start, naive_start + timedelta(hours=1))
    assert len(rows) == 4


def test_list_rejects_inverted_range():
    with pytest.raises(InvalidRequest):
        list_readings(InMemoryRepository(), NOW, NOW)


def test_list_without_start_reads_from_the_oldest():
    rows = list_readings(stored_repository(), None, NOW + timedelta(hours=1))
    assert len(rows) == 4


@pytest.mark.parametrize(
    "text, expected",
    [
        ("30m", timedelta(minutes=30)),
        ("1h", timedelta(hours=1)),
        ("24h", timedelta(hours=24)),
        ("7d", timedelta(days=7)),
    ],
)
def test_parse_period(text, expected):
    assert parse_period(text) == expected


@pytest.mark.parametrize("text", ["", "0m", "5", "m", "5x", "1.5h", "123456h"])
def test_parse_period_rejects_garbage(text):
    with pytest.raises(InvalidRequest):
        parse_period(text)


def test_resolve_range_defaults_to_everything_until_now():
    start, end = resolve_range(NOW, None, None, None)
    assert start is None
    assert end == NOW + timedelta(seconds=MAX_FUTURE_S)


def test_resolve_range_period_is_relative_to_now():
    start, end = resolve_range(NOW, None, None, "6h")
    assert start == NOW - timedelta(hours=6)
    assert end == NOW + timedelta(seconds=MAX_FUTURE_S)


def test_resolve_range_keeps_explicit_bounds():
    start, end = resolve_range(NOW, NOW - timedelta(hours=1), NOW, None)
    assert (start, end) == (NOW - timedelta(hours=1), NOW)


def test_resolve_range_rejects_period_with_bounds():
    with pytest.raises(InvalidRequest):
        resolve_range(NOW, NOW, None, "1h")


def test_dicts_use_iso_timestamps():
    rows = list_readings(stored_repository(), NOW, NOW + timedelta(hours=1))
    assert readings_to_dicts(rows)[0]["ts"] == NOW.isoformat()


def test_csv_has_header_and_blank_cells_for_missing_values():
    rows = list_readings(stored_repository(), NOW, NOW + timedelta(seconds=1))
    lines = readings_to_csv(rows).splitlines()
    assert lines[0].startswith("device_id,ts,received_at")
    assert lines[1].endswith(",,,")
    assert len(lines) == 2
