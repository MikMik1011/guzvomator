from datetime import datetime, timezone

from app.models import Reading
from app.repository import StoredReading


class InMemoryRepository:
    def __init__(self) -> None:
        self.rows: dict[tuple[str, int], StoredReading] = {}

    def insert(self, reading: Reading, received_at: datetime) -> bool:
        key = (reading.device_id, reading.ts)
        if key in self.rows:
            return False
        self.rows[key] = StoredReading(
            device_id=reading.device_id,
            ts=datetime.fromtimestamp(reading.ts, tz=timezone.utc),
            received_at=received_at,
            scan_window_s=reading.scan_window_s,
            rssi_min=reading.rssi_min,
            devices=reading.devices,
            devices_above_rssi=reading.devices_above_rssi,
            avg_rssi=reading.avg_rssi,
            temp_c=reading.temp_c,
            humidity_pct=reading.humidity_pct,
            lux=reading.lux,
        )
        return True

    def query(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        limit: int,
    ) -> list[StoredReading]:
        matching = [
            row
            for row in self.rows.values()
            if (start is None or start <= row.ts)
            and row.ts < end
            and device_id in (None, row.device_id)
        ]
        return sorted(matching, key=lambda row: row.ts)[:limit]
