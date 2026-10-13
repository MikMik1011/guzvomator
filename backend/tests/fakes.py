from collections import defaultdict
from datetime import datetime, timezone

from app.models import Reading
from app.repository import BucketSummary, StoredReading


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

    def _matching(
        self, start: datetime | None, end: datetime, device_id: str | None
    ) -> list[StoredReading]:
        matching = [
            row
            for row in self.rows.values()
            if (start is None or start <= row.ts)
            and row.ts < end
            and device_id in (None, row.device_id)
        ]
        return sorted(matching, key=lambda row: row.ts)

    def query(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        limit: int,
    ) -> list[StoredReading]:
        return self._matching(start, end, device_id)[-limit:]

    def time_span(
        self, start: datetime | None, end: datetime, device_id: str | None
    ) -> tuple[datetime, datetime] | None:
        rows = self._matching(start, end, device_id)
        return (rows[0].ts, rows[-1].ts) if rows else None

    def aggregate(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        bucket_seconds: int,
    ) -> list[BucketSummary]:
        groups: dict[tuple[str, int], list[StoredReading]] = defaultdict(list)
        for row in self._matching(start, end, device_id):
            index = int(row.ts.timestamp()) // bucket_seconds
            groups[(row.device_id, index)].append(row)
        return [
            BucketSummary(
                device_id=device,
                start=datetime.fromtimestamp(index * bucket_seconds, tz=timezone.utc),
                devices_mean=sum(r.devices for r in rows) / len(rows),
                devices_low=percentile([r.devices for r in rows], 0.1),
                devices_high=percentile([r.devices for r in rows], 0.9),
                above_mean=sum(r.devices_above_rssi for r in rows) / len(rows),
            )
            for (device, index), rows in sorted(groups.items())
        ]


def percentile(values: list[int], fraction: float) -> float:
    """Linear interpolation between closest ranks, like PostgreSQL's percentile_cont."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    below = int(position)
    above = min(below + 1, len(ordered) - 1)
    return ordered[below] + (ordered[above] - ordered[below]) * (position - below)


def stored_reading(
    device_id: str, ts: datetime, devices: int, above: int = 0
) -> StoredReading:
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


def repository_with(rows: list[StoredReading]) -> InMemoryRepository:
    repository = InMemoryRepository()
    for row in rows:
        repository.rows[(row.device_id, int(row.ts.timestamp()))] = row
    return repository
