import os
from dataclasses import dataclass
from functools import lru_cache


@dataclass(frozen=True)
class Settings:
    database_url: str
    api_key: str


def _required(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise RuntimeError(f"{name} is not set")
    return value


@lru_cache
def get_settings() -> Settings:
    return Settings(
        database_url=_required("DATABASE_URL"),
        api_key=_required("API_KEY"),
    )


@dataclass(frozen=True)
class MqttSettings:
    database_url: str
    host: str
    port: int
    user: str
    password: str
    topic: str


TLS_PORT = 8883


@lru_cache
def get_mqtt_settings() -> MqttSettings:
    return MqttSettings(
        database_url=_required("DATABASE_URL"),
        host=_required("MQTT_HOST"),
        port=int(os.environ.get("MQTT_PORT", "1883")),
        user=os.environ.get("MQTT_USER", ""),
        password=os.environ.get("MQTT_PASSWORD", ""),
        topic=os.environ.get("MQTT_TOPIC", "guzvomator/#"),
    )
