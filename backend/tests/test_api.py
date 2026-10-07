from datetime import datetime, timedelta, timezone

import pytest
from fastapi.testclient import TestClient

from app.api import app, get_repository
from app.settings import Settings, get_settings
from tests.fakes import InMemoryRepository
from tests.payloads import valid_payload

API_KEY = "test-key"
HEADERS = {"X-API-Key": API_KEY}


@pytest.fixture
def client():
    repository = InMemoryRepository()
    app.dependency_overrides[get_repository] = lambda: repository
    app.dependency_overrides[get_settings] = lambda: Settings("unused", API_KEY)
    yield TestClient(app)
    app.dependency_overrides.clear()


def recent_ts() -> int:
    return int(datetime.now(timezone.utc).timestamp()) - 60


def test_health_needs_no_key(client):
    assert client.get("/health").json() == {"status": "ok"}


def test_post_creates_then_reports_duplicate(client):
    payload = valid_payload(ts=recent_ts())
    first = client.post("/api/v1/readings", json=payload, headers=HEADERS)
    second = client.post("/api/v1/readings", json=payload, headers=HEADERS)
    assert (first.status_code, first.json()) == (201, {"status": "created"})
    assert (second.status_code, second.json()) == (200, {"status": "duplicate"})


def test_post_without_key_is_unauthorized(client):
    response = client.post("/api/v1/readings", json=valid_payload(ts=recent_ts()))
    assert response.status_code == 401


def test_post_with_wrong_key_is_unauthorized(client):
    response = client.post(
        "/api/v1/readings", json=valid_payload(ts=recent_ts()), headers={"X-API-Key": "nope"}
    )
    assert response.status_code == 401


def test_unauthorized_wins_over_invalid_body(client):
    response = client.post("/api/v1/readings", json={"junk": True})
    assert response.status_code == 401


def test_invalid_payload_is_bad_request(client):
    response = client.post("/api/v1/readings", json=valid_payload(v=2), headers=HEADERS)
    assert response.status_code == 400
    assert response.json()["detail"][0]["loc"][-1] == "v"


def test_future_reading_is_bad_request(client):
    future = int(datetime.now(timezone.utc).timestamp()) + 3600
    response = client.post("/api/v1/readings", json=valid_payload(ts=future), headers=HEADERS)
    assert response.status_code == 400


def test_get_returns_json_by_default(client):
    ts = recent_ts()
    client.post("/api/v1/readings", json=valid_payload(ts=ts), headers=HEADERS)
    response = client.get("/api/v1/readings")
    assert response.status_code == 200
    assert response.json()[0]["devices"] == 12


def test_default_range_includes_readings_slightly_ahead_of_the_server_clock(client):
    ahead = int(datetime.now(timezone.utc).timestamp()) + 60
    client.post("/api/v1/readings", json=valid_payload(ts=ahead), headers=HEADERS)
    assert len(client.get("/api/v1/readings").json()) == 1


def post_at(client, age_s: int, device_id: str = "guzvo-a1b2c3") -> int:
    ts = int(datetime.now(timezone.utc).timestamp()) - age_s
    response = client.post(
        "/api/v1/readings", json=valid_payload(ts=ts, device_id=device_id), headers=HEADERS
    )
    assert response.status_code == 201
    return ts


def returned_ages(client, **params) -> list[int]:
    now = datetime.now(timezone.utc).timestamp()
    response = client.get("/api/v1/readings", params=params)
    assert response.status_code == 200
    ages = [now - datetime.fromisoformat(row["ts"]).timestamp() for row in response.json()]
    return [round(age / 10) * 10 for age in ages]


def test_no_range_returns_everything_oldest_first(client):
    for age in (3 * 24 * 3600, 3600, 60):
        post_at(client, age)
    assert returned_ages(client) == [3 * 24 * 3600, 3600, 60]


@pytest.mark.parametrize(
    "period, expected",
    [
        ("30m", [60]),
        ("3h", [3600, 60]),
        ("24h", [3600, 60]),
        ("7d", [3 * 24 * 3600, 3600, 60]),
    ],
)
def test_period_returns_the_last_n(client, period, expected):
    for age in (3 * 24 * 3600, 3600, 60):
        post_at(client, age)
    assert returned_ages(client, period=period) == expected


@pytest.mark.parametrize(
    "period, expected",
    [
        ("1m", [30]),
        ("5m", [30, 120]),
        ("10m", [30, 120, 420]),
        ("15m", [30, 120, 420, 720]),
        ("30m", [30, 120, 420, 720, 1200]),
        ("1h", [30, 120, 420, 720, 1200, 2700]),
    ],
)
def test_short_period_presets(client, period, expected):
    for age in (2700, 1200, 720, 420, 120, 30):
        post_at(client, age)
    assert returned_ages(client, period=period) == sorted(expected, reverse=True)


def test_from_only_runs_until_now(client):
    for age in (3 * 24 * 3600, 3600, 60):
        post_at(client, age)
    since = (datetime.now(timezone.utc) - timedelta(hours=2)).isoformat()
    assert returned_ages(client, **{"from": since}) == [3600, 60]


def test_to_only_starts_at_the_oldest_reading(client):
    for age in (3 * 24 * 3600, 3600, 60):
        post_at(client, age)
    until = (datetime.now(timezone.utc) - timedelta(minutes=30)).isoformat()
    assert returned_ages(client, to=until) == [3 * 24 * 3600, 3600]


def test_from_and_to_select_a_window(client):
    for age in (3 * 24 * 3600, 3600, 60):
        post_at(client, age)
    now = datetime.now(timezone.utc)
    window = {
        "from": (now - timedelta(hours=2)).isoformat(),
        "to": (now - timedelta(minutes=30)).isoformat(),
    }
    assert returned_ages(client, **window) == [3600]


def test_period_cannot_be_combined_with_from_or_to(client):
    now = datetime.now(timezone.utc).isoformat()
    for extra in ({"from": now}, {"to": now}):
        response = client.get("/api/v1/readings", params={"period": "1h", **extra})
        assert response.status_code == 400


@pytest.mark.parametrize("period", ["0h", "h", "5x", "1.5h", "-1h", "100000m", "abc"])
def test_invalid_period_is_bad_request(client, period):
    assert client.get("/api/v1/readings", params={"period": period}).status_code == 400


def test_get_returns_csv_when_requested(client):
    client.post("/api/v1/readings", json=valid_payload(ts=recent_ts()), headers=HEADERS)
    response = client.get("/api/v1/readings", headers={"Accept": "text/csv"})
    assert response.headers["content-type"].startswith("text/csv")
    assert response.text.splitlines()[0].startswith("device_id,ts")


def test_get_filters_by_device_and_range(client):
    ts = recent_ts()
    client.post("/api/v1/readings", json=valid_payload(ts=ts), headers=HEADERS)
    client.post("/api/v1/readings", json=valid_payload(ts=ts, device_id="other"), headers=HEADERS)
    day_ago = (datetime.now(timezone.utc) - timedelta(days=2)).isoformat()
    response = client.get(
        "/api/v1/readings",
        params={"device_id": "other", "from": day_ago},
    )
    assert [row["device_id"] for row in response.json()] == ["other"]


def test_get_needs_no_key(client):
    assert client.get("/api/v1/readings").status_code == 200


def test_get_rejects_inverted_range(client):
    now = datetime.now(timezone.utc)
    response = client.get(
        "/api/v1/readings",
        params={"from": now.isoformat(), "to": (now - timedelta(hours=1)).isoformat()},
    )
    assert response.status_code == 400
