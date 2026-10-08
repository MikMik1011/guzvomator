from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator

# 2020-01-01 UTC; anything earlier comes from a clock that was never synced
MIN_VALID_TS = 1_577_836_800


class Reading(BaseModel):
    model_config = ConfigDict(extra="forbid")

    v: Literal[1]
    device_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,31}$")
    ts: int = Field(ge=MIN_VALID_TS)
    scan_window_s: int = Field(ge=1, le=3600)
    rssi_min: int = Field(ge=-127, le=0)
    devices: int = Field(ge=0, le=65535)
    devices_above_rssi: int = Field(ge=0, le=65535)
    avg_rssi: int = Field(ge=-128, le=127)
    temp_c: float | None = Field(default=None, ge=-100, le=100)
    humidity_pct: float | None = Field(default=None, ge=0, le=100)
    lux: float | None = Field(default=None, ge=0)

    @model_validator(mode="after")
    def check_above_rssi_within_total(self) -> "Reading":
        if self.devices_above_rssi > self.devices:
            raise ValueError("devices_above_rssi exceeds devices")
        return self
