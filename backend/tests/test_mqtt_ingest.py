import json
from types import SimpleNamespace

import pytest

from app.mqtt_ingest import build_client
from app.settings import MqttSettings
from tests.fakes import InMemoryRepository
from tests.payloads import valid_payload

SETTINGS = MqttSettings(
    database_url="",
    host="broker",
    port=1883,
    user="guzvo",
    password="secret",
    topic="guzvomator/#",
)


class RecordingClient:
    """Stands in for the network side of the paho client."""

    def __init__(self, client):
        self.acked = []
        self.subscriptions = []
        self.disconnected = False
        client.ack = lambda mid, qos: self.acked.append(mid)
        client.disconnect = lambda: setattr(self, "disconnected", True)
        client.subscribe = lambda topic, qos: self.subscriptions.append((topic, qos))
        self.client = client


def message(payload: bytes, mid: int = 1):
    return SimpleNamespace(payload=payload, mid=mid, qos=1, topic="guzvomator/readings")


@pytest.fixture
def repository():
    return InMemoryRepository()


def deliver(repository, payload: bytes, mid: int = 1) -> RecordingClient:
    recording = RecordingClient(build_client(SETTINGS, repository))
    recording.client.on_message(recording.client, None, message(payload, mid))
    return recording


def test_a_stored_message_is_acknowledged(repository):
    recording = deliver(repository, json.dumps(valid_payload()).encode(), mid=7)
    assert recording.acked == [7]
    assert len(repository.rows) == 1


def test_an_invalid_message_is_acknowledged_and_not_stored(repository):
    recording = deliver(repository, b"garbage", mid=8)
    assert recording.acked == [8]
    assert repository.rows == {}


def test_a_storage_failure_leaves_the_message_unacknowledged(repository):
    def fail(*args):
        raise RuntimeError("database is down")

    repository.insert = fail
    recording = deliver(repository, json.dumps(valid_payload()).encode(), mid=9)
    assert recording.acked == []
    assert recording.disconnected


def test_it_subscribes_with_qos_1_after_connecting(repository):
    recording = RecordingClient(build_client(SETTINGS, repository))
    recording.client.on_connect(
        recording.client, None, {}, SimpleNamespace(is_failure=False), None
    )
    assert recording.subscriptions == [("guzvomator/#", 1)]


def test_it_does_not_subscribe_when_the_broker_refuses(repository):
    recording = RecordingClient(build_client(SETTINGS, repository))
    recording.client.on_connect(
        recording.client, None, {}, SimpleNamespace(is_failure=True), None
    )
    assert recording.subscriptions == []
