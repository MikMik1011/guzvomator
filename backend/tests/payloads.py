NOW_TS = 1_790_000_000


def valid_payload(**overrides) -> dict:
    payload = {
        "v": 1,
        "device_id": "guzvo-a1b2c3",
        "ts": NOW_TS,
        "scan_window_s": 10,
        "rssi_min": -80,
        "devices": 12,
        "devices_above_rssi": 5,
        "avg_rssi": -83,
    }
    payload.update(overrides)
    return payload
