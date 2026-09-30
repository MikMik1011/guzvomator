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
- Wi-Fi is used only as the uplink for sending data and for NTP. No Wi-Fi probe or promiscuous-mode scanning for now.
- Wi-Fi stays connected while BLE scans. The C6 has one 2.4 GHz radio shared by coexistence arbitration, which can cost scan packets during transfers. Verify on hardware by comparing counts with and without Wi-Fi traffic.
- Build with PlatformIO. Keep every module a self-contained library under `firmware/lib/` so it also compiles in the Arduino IDE.

## Repository layout

```
firmware/
  platformio.ini
  src/main.cpp        creates the tasks; no logic beyond orchestration
  lib/
    Config/           parameter registry (arduinoConfig-style) + NVS persistence
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
1. Snapshot config; make sure Wi-Fi is connected (retry with backoff, never block the CLI).
2. BLE scan for `scan_window_s`.
3. Hash each address with the daily salt, count unique hashes, count those above `rssi_min`, compute average RSSI. Discard raw addresses.
4. Read BME280 and BH1750.
5. Build the JSON payload (see "Payload").
6. Send with the transport selected by config. If sending fails, keep a bounded queue of recent readings in RAM and send them after reconnecting.
7. Pause `scan_pause_s`.

Time comes from NTP over the always-on Wi-Fi. It provides reading timestamps and the date for the daily salt. Until the first sync, use a boot-random salt and do not send readings.

## Configuration and CLI

Reference implementations by the supervisor, which constrain the design:
- https://github.com/djherceg/arduinoConfig (GPL-3.0): parameters are registered as "pins" with id, name, variable, type, mode and change callback. Its binary command format is undocumented in the README; read `src/` and `include/` before defining the format.
- https://github.com/djherceg/arduinoCmdProc (MIT): forward-only command and argument parser. Its README has no API detail; read `src/`, `include/` and `examples/`.

License: AGPL-3.0 for the whole project, with a single `LICENSE` file in the repository root (AGPL-3.0 can be combined with the GPL-3.0 arduinoConfig). No per-file license headers or SPDX lines. Never modify or strip license notices in third-party code. To be confirmed with the supervisor.

Config parameters: `device_id`, `wifi_ssid`, `wifi_pass`, `transport` (`http` or `mqtt`), `endpoint_url`, `mqtt_host`, `mqtt_topic`, `scan_window_s`, `scan_pause_s`, `rssi_min`.

CLI commands: `list`, `get <name>`, `set <name> <value>`, `save`, `reset`, `status`, `reboot`. Every parameter change must be persisted to NVS through the change callback or an explicit `save`.

## Payload

One JSON object per reading, identical for HTTP and MQTT. No formal schema yet; the format is not frozen. Before implementing the backend endpoint, propose the fields and confirm them, then keep firmware and backend in sync.

Intended contents: node identifier, measurement time, scan window length, number of detected devices (unique salted hashes), number above the RSSI threshold, average RSSI, and optionally temperature, humidity and illuminance.

Proposed addition, to confirm: counts at a fixed set of RSSI thresholds (a sweep, aggregate only), so the threshold can be chosen offline per site without reflashing.

## Calibration

- Counts are not comparable between sites; calibration is per space. `rssi_min` is a per-site config value.
- Background devices from neighbouring rooms, floors and corridors add to the weak-RSSI band and RSSI cannot tell direction. Record an empty-room baseline per site and analyze counts against it.
- Large rooms: one node covers a zone, not the whole room. Multiple nodes (distinct `device_id`) are possible but not planned; overlapping zones would double-count and hashes are not shared between nodes.
- Headcount alone does not explain count variation (crowd turnover, phone behaviour, address rotation). Report the variance in the analysis.

## Backend

- `POST /api/v1/readings`: validate the payload, insert into PostgreSQL.
- `GET /api/v1/readings?from=&to=&device_id=`: JSON or CSV (`Accept: text/csv`).
- FastAPI, PostgreSQL, single `docker compose up` deployment.
- Keep the handler logic separate from the framework layer so it can be wrapped as an Azure Function if the supervisor requires Azure. Not yet decided.
- MQTT ingestion (Mosquitto subscriber) is optional and comes last.

## Privacy rules (hard constraints)

- Never store or transmit raw MAC addresses or any per-device identifier.
- Hash with a salt that rotates daily; keep hashes in RAM only for the duration of one window.
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
- Surface unresolved decisions instead of assuming defaults. Currently open: whether Azure is mandatory, whether ToF ground truth stays in scope, exact config format and CLI command set (depends on the supervisor's libraries), whether readings while offline are queued in RAM only or also persisted, and the size of that queue.