from dataclasses import dataclass
from datetime import datetime
from typing import Protocol

from app.models import Reading


@dataclass(frozen=True)
class StoredReading:
    device_id: str
    ts: datetime
    received_at: datetime
    scan_window_s: int
    rssi_min: int
    devices: int
    devices_above_rssi: int
    avg_rssi: int
    temp_c: float | None
    humidity_pct: float | None
    lux: float | None


class ReadingRepository(Protocol):
    def insert(self, reading: Reading, received_at: datetime) -> bool:
        """Store a reading. Returns False if (device_id, ts) already exists."""

    def query(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        limit: int,
    ) -> list[StoredReading]:
        """Readings with start <= ts < end (no lower bound if start is None), oldest first."""
