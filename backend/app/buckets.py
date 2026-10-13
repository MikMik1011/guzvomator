from dataclasses import dataclass

from app.handlers import (
    InvalidRequest,
    list_readings,
    validate_range,
)
from app.repository import BucketSummary, ReadingRepository, StoredReading

TARGET_POINTS = 150
NICE_BUCKETS_S = [10, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 21600, 86400]
UNIT_SECONDS = {"s": 1, "m": 60, "h": 3600, "d": 86400}
MAX_BUCKETS = 5_000

AUTO = "auto"
RAW = "raw"


@dataclass(frozen=True)
class PlotData:
    """Either raw readings (bucket_seconds is None) or per-device buckets."""

    bucket_seconds: int | None
    readings: list[StoredReading]
    buckets: list[BucketSummary]


def parse_bucket(text: str) -> str | int:
    """'auto', 'raw' or a size such as 30s, 5m, 1h; returns the size in seconds for the last."""
    if text in (AUTO, RAW):
        return text
    number, unit = text[:-1], text[-1:]
    if not number.isdigit() or unit not in UNIT_SECONDS or int(number) == 0:
        raise InvalidRequest("bucket must be auto, raw, or look like 30s, 5m or 1h")
    return int(number) * UNIT_SECONDS[unit]


def auto_bucket_seconds(span_seconds: float) -> int:
    wanted = span_seconds / TARGET_POINTS
    return next((step for step in NICE_BUCKETS_S if step >= wanted), NICE_BUCKETS_S[-1])


def load_plot_data(
    repository: ReadingRepository,
    start,
    end,
    device_id: str | None,
    limit: int,
    bucket: str | int,
) -> PlotData:
    """Averaging happens in the database, so a plot never depends on how many rows match."""
    if bucket == RAW:
        return PlotData(None, list_readings(repository, start, end, device_id, limit), [])

    start, end = validate_range(start, end)
    span = repository.time_span(start, end, device_id)
    if span is None:
        return PlotData(None, [], [])

    span_seconds = (span[1] - span[0]).total_seconds()
    if bucket == AUTO:
        seconds = auto_bucket_seconds(span_seconds)
    else:
        seconds = bucket
        if span_seconds / seconds > MAX_BUCKETS:
            raise InvalidRequest("bucket is too small for this range")
    return PlotData(seconds, [], repository.aggregate(start, end, device_id, seconds))
