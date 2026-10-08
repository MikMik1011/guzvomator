import pytest
from pydantic import ValidationError

from app.models import Reading
from tests.payloads import valid_payload


def test_valid_payload_is_accepted():
    reading = Reading(**valid_payload())
    assert reading.devices == 12
    assert reading.temp_c is None


def test_sensor_values_are_optional_and_validated():
    reading = Reading(**valid_payload(temp_c=22.4, humidity_pct=41.0, lux=312))
    assert reading.lux == 312


@pytest.mark.parametrize(
    "overrides",
    [
        {"v": 2},
        {"device_id": "has space"},
        {"device_id": ""},
        {"ts": 0},
        {"ts": 1_000_000},
        {"scan_window_s": 0},
        {"rssi_min": 1},
        {"devices": -1},
        {"devices_above_rssi": 13},
        {"humidity_pct": 120},
        {"lux": -1},
        {"unexpected": 1},
    ],
)
def test_invalid_payloads_are_rejected(overrides):
    with pytest.raises(ValidationError):
        Reading(**valid_payload(**overrides))


def test_missing_required_field_is_rejected():
    payload = valid_payload()
    del payload["devices"]
    with pytest.raises(ValidationError):
        Reading(**payload)
