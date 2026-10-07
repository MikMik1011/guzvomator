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
