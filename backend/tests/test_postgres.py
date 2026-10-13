import os
from datetime import datetime, timedelta, timezone

import pytest

from app.models import Reading
from tests.payloads import NOW_TS, valid_payload

DSN = os.environ.get("TEST_DATABASE_URL")

pytestmark = pytest.mark.skipif(not DSN, reason="TEST_DATABASE_URL is not set")

NOW = datetime.fromtimestamp(NOW_TS, tz=timezone.utc)


@pytest.fixture
def repository():
    from app.db import PostgresRepository

    repository = PostgresRepository(DSN)
    with repository._pool.connection() as connection:
        connection.execute("TRUNCATE readings")
    return repository


def reading(**overrides) -> Reading:
    return Reading(**valid_payload(**overrides))


def test_insert_then_duplicate(repository):
    assert repository.insert(reading(), NOW) is True
    assert repository.insert(reading(devices=99), NOW) is False


def test_query_round_trip_with_optional_values(repository):
    repository.insert(reading(temp_c=22.5, lux=300), NOW)
    rows = repository.query(NOW - timedelta(hours=1), NOW + timedelta(hours=1), None, 10)
    assert len(rows) == 1
    assert rows[0].ts == NOW
    assert rows[0].temp_c == 22.5
    assert rows[0].humidity_pct is None


def test_query_without_start_has_no_lower_bound(repository):
    repository.insert(reading(ts=1_600_000_000), NOW)
    repository.insert(reading(), NOW)
    rows = repository.query(None, NOW + timedelta(hours=1), None, 10)
    assert [r.ts.timestamp() for r in rows] == [1_600_000_000, NOW_TS]


def test_query_filters_and_orders(repository):
    for offset in (120, 0, 60):
        repository.insert(reading(ts=NOW_TS + offset), NOW)
    repository.insert(reading(device_id="other"), NOW)

    start, end = NOW - timedelta(seconds=1), NOW + timedelta(seconds=90)
    mine = repository.query(start, end, "guzvo-a1b2c3", 10)
    assert [r.ts.timestamp() - NOW_TS for r in mine] == [0, 60]
    assert len(repository.query(start, end, None, 10)) == 3
    assert len(repository.query(start, end, None, 1)) == 1


def test_query_over_the_limit_keeps_the_newest_oldest_first(repository):
    for offset in range(5):
        repository.insert(reading(ts=NOW_TS + offset), NOW)
    rows = repository.query(None, NOW + timedelta(hours=1), None, 2)
    assert [r.ts.timestamp() - NOW_TS for r in rows] == [3, 4]


def test_time_span_is_the_oldest_and_newest_match(repository):
    assert repository.time_span(None, NOW + timedelta(hours=1), None) is None
    for offset in (0, 60, 120):
        repository.insert(reading(ts=NOW_TS + offset), NOW)
    repository.insert(reading(device_id="other", ts=NOW_TS + 500), NOW)
    span = repository.time_span(None, NOW + timedelta(hours=1), "guzvo-a1b2c3")
    assert span == (NOW, NOW + timedelta(seconds=120))


def test_aggregate_buckets_per_device_with_percentiles(repository):
    for offset, devices in ((0, 4), (1, 8), (2, 8), (3, 8), (4, 40), (400, 10)):
        repository.insert(reading(ts=NOW_TS + offset, devices=devices, devices_above_rssi=2), NOW)
    repository.insert(reading(device_id="other", devices=7), NOW)

    buckets = repository.aggregate(None, NOW + timedelta(hours=1), None, 300)
    mine = [b for b in buckets if b.device_id == "guzvo-a1b2c3"]
    assert len(mine) == 2 and len(buckets) == 3
    assert mine[0].start <= NOW and mine[0].start > NOW - timedelta(seconds=300)
    assert mine[0].devices_mean == pytest.approx(13.6)
    assert mine[0].devices_low == pytest.approx(5.6)
    assert mine[0].devices_high == pytest.approx(27.2)
    assert mine[0].above_mean == 2
    assert mine[0].start < mine[1].start
