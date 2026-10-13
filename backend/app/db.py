from datetime import datetime, timezone
from pathlib import Path

from psycopg.rows import dict_row
from psycopg_pool import ConnectionPool

from app.models import Reading
from app.repository import BucketSummary, StoredReading

SCHEMA = (Path(__file__).parent / "schema.sql").read_text()

INSERT_SQL = """
INSERT INTO readings (
    device_id, ts, received_at, scan_window_s, rssi_min, devices,
    devices_above_rssi, avg_rssi, temp_c, humidity_pct, lux
) VALUES (
    %(device_id)s, %(ts)s, %(received_at)s, %(scan_window_s)s, %(rssi_min)s,
    %(devices)s, %(devices_above_rssi)s, %(avg_rssi)s, %(temp_c)s,
    %(humidity_pct)s, %(lux)s
)
ON CONFLICT (device_id, ts) DO NOTHING
"""

MATCHING = """
WHERE (%(start)s::timestamptz IS NULL OR ts >= %(start)s)
  AND ts < %(end)s
  AND (%(device_id)s::text IS NULL OR device_id = %(device_id)s)
"""

# The newest rows are picked first, then put back in time order
QUERY_SQL = f"""
SELECT * FROM (
    SELECT device_id, ts, received_at, scan_window_s, rssi_min, devices,
           devices_above_rssi, avg_rssi, temp_c, humidity_pct, lux
    FROM readings
    {MATCHING}
    ORDER BY ts DESC
    LIMIT %(limit)s
) newest
ORDER BY ts
"""

SPAN_SQL = f"SELECT min(ts), max(ts) FROM readings {MATCHING}"

AGGREGATE_SQL = f"""
SELECT device_id,
       to_timestamp(
           (floor(extract(epoch FROM ts) / %(bucket)s) * %(bucket)s)::float8
       ) AS bucket_start,
       avg(devices)::float8 AS devices_mean,
       percentile_cont(0.1) WITHIN GROUP (ORDER BY devices) AS devices_low,
       percentile_cont(0.9) WITHIN GROUP (ORDER BY devices) AS devices_high,
       avg(devices_above_rssi)::float8 AS above_mean
FROM readings
{MATCHING}
GROUP BY device_id, bucket_start
ORDER BY device_id, bucket_start
"""


class PostgresRepository:
    def __init__(self, dsn: str, max_connections: int = 4):
        self._pool = ConnectionPool(
            dsn, min_size=1, max_size=max_connections, open=False
        )
        self._pool.open(wait=True)
        with self._pool.connection() as connection:
            connection.execute(SCHEMA)

    def insert(self, reading: Reading, received_at: datetime) -> bool:
        params = reading.model_dump()
        params["ts"] = datetime.fromtimestamp(reading.ts, tz=timezone.utc)
        params["received_at"] = received_at
        with self._pool.connection() as connection:
            cursor = connection.execute(INSERT_SQL, params)
            return cursor.rowcount == 1

    def query(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        limit: int,
    ) -> list[StoredReading]:
        params = {"start": start, "end": end, "device_id": device_id, "limit": limit}
        with self._pool.connection() as connection:
            with connection.cursor(row_factory=dict_row) as cursor:
                cursor.execute(QUERY_SQL, params)
                return [StoredReading(**row) for row in cursor.fetchall()]

    def time_span(
        self, start: datetime | None, end: datetime, device_id: str | None
    ) -> tuple[datetime, datetime] | None:
        params = {"start": start, "end": end, "device_id": device_id}
        with self._pool.connection() as connection:
            oldest, newest = connection.execute(SPAN_SQL, params).fetchone()
        return None if oldest is None else (oldest, newest)

    def aggregate(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        bucket_seconds: int,
    ) -> list[BucketSummary]:
        params = {
            "start": start,
            "end": end,
            "device_id": device_id,
            "bucket": bucket_seconds,
        }
        with self._pool.connection() as connection:
            with connection.cursor(row_factory=dict_row) as cursor:
                cursor.execute(AGGREGATE_SQL, params)
                return [
                    BucketSummary(
                        device_id=row["device_id"],
                        start=row["bucket_start"],
                        devices_mean=row["devices_mean"],
                        devices_low=row["devices_low"],
                        devices_high=row["devices_high"],
                        above_mean=row["above_mean"],
                    )
                    for row in cursor.fetchall()
                ]
