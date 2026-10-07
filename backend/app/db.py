from datetime import datetime, timezone
from pathlib import Path

from psycopg.rows import dict_row
from psycopg_pool import ConnectionPool

from app.models import Reading
from app.repository import StoredReading

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

QUERY_SQL = """
SELECT device_id, ts, received_at, scan_window_s, rssi_min, devices,
       devices_above_rssi, avg_rssi, temp_c, humidity_pct, lux
FROM readings
WHERE (%(start)s::timestamptz IS NULL OR ts >= %(start)s)
  AND ts < %(end)s
  AND (%(device_id)s::text IS NULL OR device_id = %(device_id)s)
ORDER BY ts
LIMIT %(limit)s
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
