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
- SNTP provides the clock, used for TLS certificate validation and for reading timestamps. The server defaults to `pool.ntp.org` and is configurable (`ntp_server`). HTTPS verifies the server certificate. Which root certificate(s) to embed, or whether to use the ESP32 certificate bundle, is open. Plain HTTP is allowed for local development.
- Up to three saved Wi-Fi profiles (WPA2-Personal, SSID and password). The node connects to the strongest visible known network. Enterprise networks such as eduroam are not supported.
- Wi-Fi stays connected while BLE scans. The C6 has one 2.4 GHz radio shared by coexistence arbitration, which can cost scan packets during transfers. Verify on hardware by comparing counts with and without Wi-Fi traffic.
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
backend/
  app/                FastAPI service
  docker-compose.yml  backend + PostgreSQL
docs/
  PLAN.md             seminar proposal (Serbian)
```

## Firmware runtime

Always on, two FreeRTOS tasks (the C6 is single-core, so they are time-sliced):
- CLI task: reads serial and edits config. Blocked on input most of the time.
- Worker task: runs the measurement cycle below, repeatedly.

Config is shared through a mutex. The worker takes a snapshot at the start of each cycle, so a change applies on the next cycle. Wi-Fi settings changed from the CLI make the worker reconnect.

Worker cycle:
1. Snapshot config; make sure Wi-Fi is connected to one of the saved profiles, strongest visible first (retry with backoff, never block the CLI), and that the clock has been synced at least once. No measurement starts before the first sync.
2. BLE scan for `scan_window_s`.
3. Hash each address with a fresh random salt generated for this window, count unique hashes, count those above `rssi_min`, compute average RSSI. Discard raw addresses.
4. Read BME280 and BH1750.
5. Build the JSON payload (see "Payload").
6. Send with the transport selected by config. If sending fails, keep a bounded queue of recent readings in RAM and send them after reconnecting, each with its own measurement time.
7. Pause `scan_pause_s`.

Each reading carries `ts`, its measurement time in UTC epoch seconds from the SNTP-synced clock. The clock keeps running if Wi-Fi drops and SNTP resyncs when it returns. The backend also stores its own receive time for diagnostics.

## Configuration and CLI

The supervisor's libraries:
- https://github.com/djherceg/arduinoConfig (GPL-3.0): marked outdated by its author (`docs/readme.txt`, 5.8.2025). Not a dependency, and no code is copied from it. Our own registry in `Config/` follows its design: parameters with a name, type, value variable and change callback. It has no persistence, so NVS storage is ours. From reading the source: its include names differ in case from the real file names (fails to build on Linux) and the repo is a PlatformIO app, not a library.
- https://github.com/djherceg/arduinoCmdProc (MIT, current): a dependency of the `Cli` library only, used for tokenizing and integer/string parsing. Its README has no API detail; read `src/` and `examples/`. Avoid its float parser, range-check parsed integers (it does not detect overflow), and note that an empty `""` argument is dropped.

License: AGPL-3.0 for the whole project, with a single `LICENSE` file in the repository root (arduinoCmdProc is MIT, which is compatible). No per-file license headers or SPDX lines. Never modify or strip license notices in third-party code. To be confirmed with the supervisor.

Config parameters: `device_id`, `wifi0_ssid`, `wifi0_pass`, `wifi1_ssid`, `wifi1_pass`, `wifi2_ssid`, `wifi2_pass` (an empty SSID means an unused slot), `transport` (`http` or `mqtt`), `endpoint_url`, `api_key` (secret, masked in `list` and `get`), `mqtt_host`, `mqtt_topic`, `ntp_server`, `scan_window_s`, `scan_pause_s`, `rssi_min`.

CLI commands: `list`, `get <name>`, `set <name> <value>`, `save`, `reset`, `status`, `reboot`. Every parameter change must be persisted to NVS through the change callback or an explicit `save`.

## Payload

One JSON object per reading, identical for HTTP and MQTT. No formal schema yet; the format is not frozen. Before implementing the backend endpoint, propose the fields and confirm them, then keep firmware and backend in sync.

Intended contents: node identifier, `ts` (measurement time, UTC epoch seconds), scan window length, number of detected devices (unique salted hashes), number above the RSSI threshold, average RSSI, and optionally temperature, humidity and illuminance.

The backend treats `device_id + ts` as the duplicate-detection key, so a retried send is idempotent.

## Calibration

- Counts are not comparable between sites; calibration is per space. `rssi_min` is a per-site config value.
- Background devices from neighbouring rooms, floors and corridors add to the weak-RSSI band and RSSI cannot tell direction. Record an empty-room baseline per site and analyze counts against it.
- Large rooms: one node covers a zone, not the whole room. Multiple nodes (distinct `device_id`) are possible but not planned; overlapping zones would double-count and hashes are not shared between nodes.
- Headcount alone does not explain count variation (crowd turnover, phone behaviour, address rotation). Report the variance in the analysis.

## Backend

- `POST /api/v1/readings`: validate the payload, insert into PostgreSQL. `201` stored, `200` duplicate (same `device_id` and `ts`, treated as success), `400` invalid, `401` bad key.
- `GET /api/v1/readings?from=&to=&device_id=`: JSON or CSV (`Accept: text/csv`). `from` and `to` are ISO 8601 and each is optional (no `from` means from the oldest reading, no `to` means until now). `period` (`1m`, `5m`, `10m`, `15m`, `30m`, `1h`, `3h`, `6h`, `12h`, `24h`, `7d`, or any N of minutes, hours or days) selects the last N and cannot be combined with `from` or `to`. With no range parameters everything is returned, up to `limit` (default 10000, max 100000, oldest first).
- Only `POST /api/v1/readings` needs the `X-API-Key` header. `GET /api/v1/readings` and `GET /health` are public (readings are aggregates only).
- FastAPI, PostgreSQL, single `docker compose up` deployment. Python 3.12 managed with uv (`pyproject.toml`, `uv.lock`). Configuration comes from the `DATABASE_URL` and `API_KEY` environment variables.
- Layout: `app/handlers.py` (plain logic), `app/db.py` (PostgreSQL), `app/api.py` (FastAPI adapter), `app/schema.sql` (applied on first connection). `make backend-test` runs the tests. `make backend-dev` starts PostgreSQL in Docker (`docker-compose.dev.yml` publishes it on localhost) and runs the API with `uvicorn --reload` on the host.
- Keep the handler logic separate from the framework layer so it can be wrapped as an Azure Function if the supervisor requires Azure. Not yet decided.
- MQTT ingestion (Mosquitto subscriber) is optional and comes last.

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
- Surface unresolved decisions instead of assuming defaults. Currently open: whether Azure is mandatory, whether ToF ground truth stays in scope, exact config format and CLI command set (depends on the supervisor's libraries), whether readings while offline are queued in RAM only or also persisted, and the size of that queue, how Wi-Fi profiles are managed from the CLI (`set wifiN_*` or dedicated `wifi add/del/list` commands).