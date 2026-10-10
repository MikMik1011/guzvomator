import hmac
from datetime import datetime, timezone
from functools import lru_cache
from typing import Annotated

from fastapi import Depends, FastAPI, Header, HTTPException, Query, Request, Response
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse, PlainTextResponse

from app.db import PostgresRepository
from app.handlers import (
    DEFAULT_LIMIT,
    MAX_LIMIT,
    InvalidRequest,
    StoreOutcome,
    list_readings,
    readings_to_csv,
    readings_to_dicts,
    resolve_range,
    store_reading,
)
from app.models import Reading
from app.plotting import AUTO, parse_bucket, render_devices_plot
from app.repository import ReadingRepository, StoredReading
from app.settings import Settings, get_settings

app = FastAPI(title="Guzvomator")


@lru_cache
def get_repository() -> ReadingRepository:
    return PostgresRepository(get_settings().database_url)


def require_api_key(
    settings: Annotated[Settings, Depends(get_settings)],
    x_api_key: Annotated[str | None, Header()] = None,
) -> None:
    valid = x_api_key is not None and hmac.compare_digest(
        x_api_key.encode(), settings.api_key.encode()
    )
    if not valid:
        raise HTTPException(status_code=401, detail="invalid api key")


@app.exception_handler(RequestValidationError)
async def handle_validation_error(request: Request, exc: RequestValidationError):
    errors = [{"loc": list(e["loc"]), "msg": e["msg"]} for e in exc.errors()]
    return JSONResponse(status_code=400, content={"detail": errors})


@app.exception_handler(InvalidRequest)
async def handle_invalid_request(request: Request, exc: InvalidRequest):
    return JSONResponse(status_code=400, content={"detail": str(exc)})


@app.get("/health")
def health() -> dict:
    return {"status": "ok"}


@app.post("/api/v1/readings", dependencies=[Depends(require_api_key)])
def create_reading(
    reading: Reading,
    response: Response,
    repository: Annotated[ReadingRepository, Depends(get_repository)],
) -> dict:
    outcome = store_reading(repository, reading, datetime.now(timezone.utc))
    response.status_code = 201 if outcome is StoreOutcome.CREATED else 200
    return {"status": outcome.value}


def select_readings(
    repository: ReadingRepository,
    from_: datetime | None,
    to: datetime | None,
    period: str | None,
    device_id: str | None,
    limit: int,
) -> list[StoredReading]:
    start, end = resolve_range(datetime.now(timezone.utc), from_, to, period)
    return list_readings(repository, start, end, device_id, limit)


@app.get("/api/v1/readings")
def get_readings(
    request: Request,
    repository: Annotated[ReadingRepository, Depends(get_repository)],
    from_: Annotated[datetime | None, Query(alias="from")] = None,
    to: datetime | None = None,
    period: Annotated[str | None, Query(pattern=r"^\d{1,5}[mhd]$")] = None,
    device_id: str | None = None,
    limit: Annotated[int, Query(ge=1, le=MAX_LIMIT)] = DEFAULT_LIMIT,
):
    readings = select_readings(repository, from_, to, period, device_id, limit)

    if "text/csv" in request.headers.get("accept", ""):
        return PlainTextResponse(readings_to_csv(readings), media_type="text/csv")
    return readings_to_dicts(readings)


@app.get("/api/v1/plot")
def get_plot(
    repository: Annotated[ReadingRepository, Depends(get_repository)],
    from_: Annotated[datetime | None, Query(alias="from")] = None,
    to: datetime | None = None,
    period: Annotated[str | None, Query(pattern=r"^\d{1,5}[mhd]$")] = None,
    device_id: str | None = None,
    limit: Annotated[int, Query(ge=1, le=MAX_LIMIT)] = DEFAULT_LIMIT,
    bucket: Annotated[str, Query(pattern=r"^(auto|raw|\d{1,5}[smhd])$")] = AUTO,
):
    readings = select_readings(repository, from_, to, period, device_id, limit)
    return Response(
        render_devices_plot(readings, parse_bucket(bucket)), media_type="image/png"
    )
