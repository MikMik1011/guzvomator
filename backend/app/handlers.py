import csv
import io
import re
from datetime import datetime, timedelta, timezone
from enum import Enum

from app.models import Reading
from app.repository import ReadingRepository, StoredReading

MAX_FUTURE_S = 300
DEFAULT_LIMIT = 10_000
MAX_LIMIT = 100_000

PERIOD_PATTERN = re.compile(r"^(\d{1,5})([mhd])$")
PERIOD_UNITS = {"m": "minutes", "h": "hours", "d": "days"}

CSV_COLUMNS = [
    "device_id",
    "ts",
    "received_at",
    "scan_window_s",
    "rssi_min",
    "devices",
    "devices_above_rssi",
    "avg_rssi",
    "temp_c",
    "humidity_pct",
    "lux",
]


class InvalidRequest(ValueError):
    pass


class StoreOutcome(Enum):
    CREATED = "created"
    DUPLICATE = "duplicate"


def store_reading(
    repository: ReadingRepository, reading: Reading, now: datetime
) -> StoreOutcome:
    if reading.ts > now.timestamp() + MAX_FUTURE_S:
        raise InvalidRequest("ts is in the future")
    inserted = repository.insert(reading, now)
    return StoreOutcome.CREATED if inserted else StoreOutcome.DUPLICATE


def parse_period(text: str) -> timedelta:
    match = PERIOD_PATTERN.match(text)
    if match is None or int(match.group(1)) == 0:
        raise InvalidRequest("period must look like 30m, 6h or 7d")
    return timedelta(**{PERIOD_UNITS[match.group(2)]: int(match.group(1))})


def resolve_range(
    now: datetime,
    start: datetime | None,
    end: datetime | None,
    period: str | None,
) -> tuple[datetime | None, datetime]:
    """No start means from the oldest reading, no end means until now."""
    default_end = now + timedelta(seconds=MAX_FUTURE_S)
    if period is not None:
        if start is not None or end is not None:
            raise InvalidRequest("period cannot be combined with from or to")
        return now - parse_period(period), default_end
    return (
        None if start is None else _as_utc(start),
        default_end if end is None else _as_utc(end),
    )


def list_readings(
    repository: ReadingRepository,
    start: datetime | None,
    end: datetime,
    device_id: str | None = None,
    limit: int = DEFAULT_LIMIT,
) -> list[StoredReading]:
    start, end = (None if start is None else _as_utc(start)), _as_utc(end)
    if start is not None and start >= end:
        raise InvalidRequest("from must be before to")
    return repository.query(start, end, device_id, min(limit, MAX_LIMIT))


def readings_to_dicts(readings: list[StoredReading]) -> list[dict]:
    return [_row(reading) for reading in readings]


def readings_to_csv(readings: list[StoredReading]) -> str:
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=CSV_COLUMNS, lineterminator="\n")
    writer.writeheader()
    for reading in readings:
        writer.writerow({k: "" if v is None else v for k, v in _row(reading).items()})
    return output.getvalue()


def _row(reading: StoredReading) -> dict:
    return {
        "device_id": reading.device_id,
        "ts": reading.ts.isoformat(),
        "received_at": reading.received_at.isoformat(),
        "scan_window_s": reading.scan_window_s,
        "rssi_min": reading.rssi_min,
        "devices": reading.devices,
        "devices_above_rssi": reading.devices_above_rssi,
        "avg_rssi": reading.avg_rssi,
        "temp_c": reading.temp_c,
        "humidity_pct": reading.humidity_pct,
        "lux": reading.lux,
    }


def _as_utc(value: datetime) -> datetime:
    if value.tzinfo is None:
        return value.replace(tzinfo=timezone.utc)
    return value.astimezone(timezone.utc)
