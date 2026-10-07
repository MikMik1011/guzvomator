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
