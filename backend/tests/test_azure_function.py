import asyncio
import json
from datetime import datetime, timezone
from pathlib import Path

import pytest

func = pytest.importorskip("azure.functions")

from app.api import app as fastapi_app  # noqa: E402
from app.api import get_repository  # noqa: E402
from app.settings import Settings, get_settings  # noqa: E402
from tests.fakes import InMemoryRepository  # noqa: E402
from tests.payloads import valid_payload  # noqa: E402

API_KEY = "test-key"
BACKEND = Path(__file__).parent.parent


class Context:
    invocation_id = "test"
    function_name = "http_app_func"
    function_directory = "."
    trace_context = None
    retry_context = None
    thread_local_storage = None


@pytest.fixture(scope="module")
def user_function():
    import function_app

    # The library registers names on the first listing, so ask only once
    (function,) = function_app.app.get_functions()
    return function.get_user_function()


@pytest.fixture
def handler(user_function):
    repository = InMemoryRepository()
    fastapi_app.dependency_overrides[get_repository] = lambda: repository
    fastapi_app.dependency_overrides[get_settings] = lambda: Settings("unused", API_KEY)
    yield user_function
    fastapi_app.dependency_overrides.clear()


def call(handler, method, path, params=None, body=None, key=None):
    request = func.HttpRequest(
        method=method,
        url=path,
        headers={"X-API-Key": key, "Content-Type": "application/json"} if key else {},
        params=params or {},
        body=json.dumps(body).encode() if body is not None else b"",
    )
    return asyncio.run(handler(request, Context()))


def recent_ts() -> int:
    return int(datetime.now(timezone.utc).timestamp()) - 60


def test_health_through_the_functions_handler(handler):
    response = call(handler, "GET", "/health")
    assert response.status_code == 200
    assert json.loads(response.get_body()) == {"status": "ok"}


def test_a_reading_can_be_posted_and_read_back(handler):
    payload = valid_payload(ts=recent_ts())
    created = call(handler, "POST", "/api/v1/readings", body=payload, key=API_KEY)
    assert created.status_code == 201

    listed = call(handler, "GET", "/api/v1/readings", params={"period": "1h"})
    assert [row["device_id"] for row in json.loads(listed.get_body())] == [payload["device_id"]]


def test_posting_without_the_key_is_refused(handler):
    response = call(handler, "POST", "/api/v1/readings", body=valid_payload(ts=recent_ts()))
    assert response.status_code == 401


def test_the_plot_is_served_as_an_image(handler):
    call(handler, "POST", "/api/v1/readings", body=valid_payload(ts=recent_ts()), key=API_KEY)
    response = call(handler, "GET", "/api/v1/plot", params={"period": "1h"})
    assert response.status_code == 200
    assert response.get_body().startswith(b"\x89PNG")


def test_routes_are_not_prefixed_with_api():
    host = json.loads((BACKEND / "host.json").read_text())
    assert host["extensions"]["http"]["routePrefix"] == ""
