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


@dataclass(frozen=True)
class BucketSummary:
    """Readings of one device inside one fixed time bucket, averaged."""

    device_id: str
    start: datetime
    devices_mean: float
    devices_low: float
    devices_high: float
    above_mean: float


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
        """Readings with start <= ts < end (no lower bound if start is None), oldest first.

        If more than `limit` match, these are the newest `limit` of them."""

    def time_span(
        self, start: datetime | None, end: datetime, device_id: str | None
    ) -> tuple[datetime, datetime] | None:
        """Oldest and newest matching timestamps, or None if nothing matches."""

    def aggregate(
        self,
        start: datetime | None,
        end: datetime,
        device_id: str | None,
        bucket_seconds: int,
    ) -> list[BucketSummary]:
        """Matching readings averaged per device into buckets, ordered by device then time.

        A bucket starts at a multiple of `bucket_seconds` since the epoch and carries
        the mean, the 10th and the 90th percentile of `devices`, and the mean of
        `devices_above_rssi`."""
