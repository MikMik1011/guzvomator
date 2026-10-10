# CLAUDE.md

Project: Guzvomator, a FOSS, self-hosted room occupancy estimation system for a faculty reading room (IoT seminar project, PMF UNS). Single author. Supervisor: Đ. Herceg (DMI, UNS).

The seminar proposal (Serbian) is in `docs/PLAN.md`. This file is the engineering summary. If they disagree, ask before changing either.

## What is being built

An ESP32 node passively scans BLE advertising packets, counts unique devices per short time window, and sends only aggregated counts plus environmental readings to a backend. Raw device addresses never leave the device. Occupancy is estimated from the counts and calibrated against manual counting.

## Scope

In scope:
- Firmware: BLE scanning, windowed counting, salted hashing, RSSI filtering, BME280 + BH1750 readings, persistent config, serial CLI, HTTP transport, MQTT transport.
- Backend: one ingest endpoint, PostgreSQL storage, read/export endpoint, Docker Compose deployment.

Out of scope unless explicitly requested: web dashboard, InfluxDB, ToF ground-truth sensor (VL53L1X), Wi-Fi probe scanning, web config portal, battery/deep-sleep tuning, Zigbee, forecasting (NeuralProphet).

## Hardware and toolchain

- Board: Seeed Studio XIAO ESP32-C6 (PlatformIO board `seeed_xiao_esp32c6`, Arduino framework).
- Sensors on I2C: BME280 (temperature, humidity), BH1750 (light).
- The node is USB-powered and always on; there is no deep sleep.
- Wi-Fi is used as the uplink for sending data and for SNTP. No Wi-Fi probe or promiscuous-mode scanning.
- SNTP provides the clock, used for TLS certificate validation and for reading timestamps. The server defaults to `pool.ntp.org` and is configurable (`ntp_server`). HTTPS verifies the server certificate against the ESP32 core's built-in bundle of trusted root certificates, so a certificate from a public CA (for example Let's Encrypt, as served by a reverse proxy on a VPS) works without embedding anything; private or self-signed CAs are not supported. Plain HTTP is allowed for local development. A TLS handshake needs a deep stack, so the scan task has 12 KB, and one connection is reused for all readings sent in a flush.
- Up to three saved Wi-Fi profiles (WPA2-Personal, SSID and password). The node connects to the strongest visible known network. Enterprise networks such as eduroam are not supported.
- Wi-Fi stays connected while BLE scans. The C6 has one 2.4 GHz radio shared by coexistence arbitration, which can cost scan packets during transfers. Verify on hardware by comparing counts with and without Wi-Fi traffic.
- The clock counts as synced only after SNTP set it in this boot. The ESP32 keeps its time across soft resets, so a stale time must not pass as synced.
- Build with PlatformIO. Keep every module a self-contained library under `firmware/lib/` so it also compiles in the Arduino IDE.

## Repository layout

```
firmware/
  platformio.ini
  src/main.cpp        creates the tasks; no logic beyond orchestration
  lib/
    Config/           parameter registry (own implementation, modeled on arduinoConfig) + NVS persistence
    Cli/              serial commands (arduinoCmdProc)
    Scanner/          BLE scan, windowed counting, salted hashing
    Sensors/          ISensor interface; BME280, BH1750 implementations
    Transport/        ITransport; HttpTransport, MqttTransport
    Payload/          serialization of readings to JSON
    Net/              Uplink: Wi-Fi profiles with reconnect backoff, SNTP clock
backend/
  app/                FastAPI service
  docker-compose.yml  backend + PostgreSQL
docs/
  PLAN.md             seminar proposal (Serbian)
```

## Firmware runtime

Always on, three FreeRTOS tasks (the C6 is single-core, so they are time-sliced):
- CLI task: reads serial and edits config. Blocked on input most of the time.
- Uplink task: keeps Wi-Fi connected to one of the saved profiles (strongest visible known network first, retry with backoff of 5 s doubling to 60 s) and runs SNTP. A reconnect can take several seconds, which is why it has its own task: it never delays the CLI or the scan.
- Worker task: runs the measurement cycle below, repeatedly.

Config is shared through a mutex. The worker takes a snapshot at the start of each cycle, so a change applies on the next cycle. The uplink task reapplies Wi-Fi and NTP settings whenever the config version changes (after `save` or `reset`) and reconnects.

The firmware uses the `huge_app.csv` partition table (3 MB application, no OTA), because Wi-Fi plus BLE no longer fits the default 1.25 MB. The NVS partition keeps its default offset and size.

Worker cycle:
1. Snapshot config and check that the clock has been synced at least once. No measurement starts before the first sync.
2. BLE scan for `scan_window_s`.
3. Hash each address with a fresh random salt generated for this window, count unique hashes, count those above `rssi_min`, compute average RSSI. Discard raw addresses.
4. Read BME280 and BH1750.
5. Build the JSON payload (see "Payload").
6. Queue the reading (RAM only, 20 readings, the oldest is dropped when full) and send the queue oldest first with the transport selected by config. HTTP `200` or `201` removes a reading. `400` drops it for good, because the backend says the payload is invalid. Anything else (no network, timeout, `5xx`, wrong key or URL) keeps it and pauses sending with a backoff of 5 s doubling to 60 s. With `mqtt` each reading is published with QoS 1 and counts as sent when the broker acknowledges it; the broker cannot reject a payload, so there is no `400` equivalent and invalid readings are dropped by the backend subscriber. A refused login, a missing acknowledgement or a connection error keeps the reading and backs off like HTTP. The MQTT client is the ESP-IDF `esp-mqtt` that ships with the Arduino core (no extra library); it connects when a flush starts and disconnects when it ends. Port 8883 verifies the broker certificate against the built-in bundle, like HTTPS.
7. Pause `scan_pause_s`.

Each reading carries `ts`, its measurement time in UTC epoch seconds from the SNTP-synced clock. The clock keeps running if Wi-Fi drops and SNTP resyncs when it returns. The backend also stores its own receive time for diagnostics.

## Configuration and CLI

The supervisor's libraries:
- https://github.com/djherceg/arduinoConfig (GPL-3.0): marked outdated by its author (`docs/readme.txt`, 5.8.2025). Not a dependency, and no code is copied from it. Our own registry in `Config/` follows its design: parameters with a name, type, value variable and change callback. It has no persistence, so NVS storage is ours. From reading the source: its include names differ in case from the real file names (fails to build on Linux) and the repo is a PlatformIO app, not a library.
- https://github.com/djherceg/arduinoCmdProc (MIT, current): a dependency of the `Cli` library only, used for tokenizing and integer/string parsing. Its README has no API detail; read `src/` and `examples/`. Avoid its float parser, range-check parsed integers (it does not detect overflow), and note that an empty `""` argument is dropped.

License: AGPL-3.0 for the whole project, with a single `LICENSE` file in the repository root (arduinoCmdProc is MIT, which is compatible). No per-file license headers or SPDX lines. Never modify or strip license notices in third-party code. To be confirmed with the supervisor.

Config parameters: `device_id`, `wifi0_ssid`, `wifi0_pass`, `wifi1_ssid`, `wifi1_pass`, `wifi2_ssid`, `wifi2_pass` (an empty SSID means an unused slot), `transport` (`http` or `mqtt`), `endpoint_url`, `api_key` (secret, masked in `list` and `get`), `mqtt_host`, `mqtt_port` (default 1883; 8883 means TLS), `mqtt_user`, `mqtt_pass` (secret), `mqtt_topic` (default `guzvomator/readings`), `ntp_server`, `scan_window_s`, `scan_pause_s`, `rssi_min`.

CLI commands: `list`, `get <name>`, `set <name> [value]` (no value clears the field), `save`, `reset`, `status` (Wi-Fi, clock, send queue and counters, unsaved changes, uptime, free heap), `reboot`, `wifi_scan` (lists visible networks with signal, channel and security; blocks for a few seconds and disturbs BLE scanning while it runs), `help`. Every parameter change must be persisted to NVS through the change callback or an explicit `save`.

The CLI line editor (`LineBuffer`) acts on backspace and Delete, turns tabs into spaces, and skips arrow-key escape sequences, so editing keys never end up in a value. Text values containing control characters are rejected, including when loaded from NVS. `list`, `get` and `wifi_scan` print values that have leading or trailing spaces or non-ASCII bytes quoted with `\xNN` escapes, so hidden characters are visible.

## Payload

One JSON object per reading, identical for HTTP and MQTT. The format below is implemented in `firmware/lib/Payload` and validated by `backend/app/models.py`; keep the two in sync. `v` stays `1` for the course of this project.

```json
{"v":1,"device_id":"guzvo-a1b2c3","ts":1790000000,"scan_window_s":10,"rssi_min":-80,
 "devices":12,"devices_above_rssi":5,"avg_rssi":-83,
 "temp_c":22.4,"humidity_pct":41.0,"lux":312.0}
```

- `ts`: measurement time, UTC epoch seconds. `rssi_min`: the threshold used for `devices_above_rssi`. `devices`: unique salted hashes in the window.
- `temp_c`, `humidity_pct` and `lux` are optional and left out when a sensor has no value. Sensors arrive in step 4.
- The firmware refuses to build a payload for a `device_id` that is not letters, digits, `-` or `_`, so no JSON escaping is needed.

The backend treats `device_id + ts` as the duplicate-detection key, so a retried send is idempotent.

## Calibration

- Counts are not comparable between sites; calibration is per space. `rssi_min` is a per-site config value.
- Background devices from neighbouring rooms, floors and corridors add to the weak-RSSI band and RSSI cannot tell direction. Record an empty-room baseline per site and analyze counts against it.
- Large rooms: one node covers a zone, not the whole room. Multiple nodes (distinct `device_id`) are possible but not planned; overlapping zones would double-count and hashes are not shared between nodes.
- Headcount alone does not explain count variation (crowd turnover, phone behaviour, address rotation). Report the variance in the analysis.

## Backend

- `POST /api/v1/readings`: validate the payload, insert into PostgreSQL. `201` stored, `200` duplicate (same `device_id` and `ts`, treated as success), `400` invalid, `401` bad key.
- `GET /api/v1/readings?from=&to=&device_id=`: JSON or CSV (`Accept: text/csv`). `from` and `to` are ISO 8601 and each is optional (no `from` means from the oldest reading, no `to` means until now). `period` (`1m`, `5m`, `10m`, `15m`, `30m`, `1h`, `3h`, `6h`, `12h`, `24h`, `7d`, or any N of minutes, hours or days) selects the last N and cannot be combined with `from` or `to`. With no range parameters everything is returned, up to `limit` (default 10000, max 100000, oldest first).
- `GET /api/v1/plot`: a PNG time plot with the same `from`, `to`, `period`, `device_id` and `limit` parameters as the readings endpoint, in two stacked panels (devices per window, and devices above the RSSI threshold, each with its own scale), one colour per device. `bucket` sets the averaging: `auto` (default) picks a size from 10 s to 1 day that gives about 150 points, `raw` draws every window, or give a size such as `30s`, `5m` or `1h`. Averaged plots show a band from the 10th to the 90th percentile, and a silence of more than three buckets is drawn as a gap. An empty range returns an image saying so. Drawn with matplotlib's object API (no pyplot, which is not thread safe) in `app/plotting.py`; the Docker image sets `MPLCONFIGDIR` because the non-root user has no home directory.
- Only `POST /api/v1/readings` needs the `X-API-Key` header. `GET /api/v1/readings`, `GET /api/v1/plot` and `GET /health` are public (readings are aggregates only).
- FastAPI, PostgreSQL, single `docker compose up` deployment. Python 3.12 managed with uv (`pyproject.toml`, `uv.lock`). Configuration comes from the `DATABASE_URL` and `API_KEY` environment variables.
- Layout: `app/handlers.py` (plain logic), `app/db.py` (PostgreSQL), `app/api.py` (FastAPI adapter), `app/schema.sql` (applied on first connection). `make backend-test` runs the tests. `make backend-dev` (add `API_HOST=0.0.0.0` so other devices such as the node can reach it) starts PostgreSQL in Docker (`docker-compose.dev.yml` publishes it on localhost) and runs the API with `uvicorn --reload` on the host.
- For HTTPS, run the backend behind a reverse proxy that terminates TLS (Caddy, nginx or Traefik) with a certificate from a public CA. uvicorn keeps speaking plain HTTP behind it.
- Keep the handler logic separate from the framework layer so it can be wrapped as an Azure Function if the supervisor requires Azure. Not yet decided.
- MQTT ingestion: `app/mqtt_ingest.py` is a separate process (`python -m app.mqtt_ingest`, the `ingest` service in `docker-compose.yml`) that subscribes to `MQTT_TOPIC` (default `guzvomator/#`) with QoS 1 and a persistent session, and stores readings through the same `store_reading` logic as HTTP (`ingest_message` in `handlers.py`). A message is acknowledged only after it was handled: invalid ones are logged and dropped, a storage failure exits the process without acknowledging so the broker redelivers it after the restart. Mosquitto (`mosquitto` service, config in `backend/mosquitto/`) requires a login created from `MQTT_USER` and `MQTT_PASSWORD` at start. The compose file publishes port 1883 in plain text; for a broker reachable from the internet, TLS on 8883 still has to be set up (not done, and not in `docker-compose.prod.yml`).

## Privacy rules (hard constraints)

- Never store or transmit raw MAC addresses or any per-device identifier.
- Hash with a fresh random salt for every scan window; the salt is never stored or sent and is wiped with the hashes at the end of the window. This is stricter than daily rotation.
- No cameras.
- MAC randomization is a known core limitation. Do not hide it; it is documented in the methodology.

## Order of work

Do one step at a time and stop for review after each.

1. Firmware: BLE scan, windows, hashing, aggregation.
2. Config registry, NVS persistence, CLI.
3. HTTP transport and backend endpoint with PostgreSQL and Docker Compose.
4. BME280 and BH1750 readings in the payload.
5. MQTT transport behind the same `ITransport` interface (first to cut if time runs short).
6. Data collection in the reading room, calibration, analysis.

## Working conventions

- Keep responses concise and technically precise.
- Code: clean, self-documenting, readable, maintainable and reusable. Prefer clear names and small functions over comments. Comment only when necessary (the why, a non-obvious constraint), never to narrate what the code does, and keep comments short and plain.
- Git: never run git write operations (add, commit, push, branch, remote, reset, etc.) unless explicitly asked for that specific operation. Never commit automatically.
- Commit messages: a single conventional-commit header only (`type(scope): summary`), no body, no trailers. Never add `Co-Authored-By` or any other attribution line.
- Surface unresolved decisions instead of assuming defaults. Currently open: whether Azure is mandatory, whether ToF ground truth stays in scope, exact config format and CLI command set (depends on the supervisor's libraries), how Wi-Fi profiles are managed from the CLI (`set wifiN_*` or dedicated `wifi add/del/list` commands).