```markdown
# AI agent quick-start: Talking Thermometer (BlindTempChecker)

This repo is a compact assistive-temperature project with three cooperating parts:
- Firmware (Arduino/ESP): `BlindTempChecker/arduino/` — reads sensors (DS18B20 or MLX90614), outputs newline-delimited JSON on Serial.
- Server (Node.js): `BlindTempChecker/server/` — opens the serial port, parses JSON, broadcasts via WebSocket and exposes HTTP endpoints.
- Web UI: `BlindTempChecker/web/` — accessible front-end with speech feedback and keyboard shortcuts.

Quick facts for editing and tests:
- Serial JSON is newline-terminated and uses 115200 baud by default. See `arduino/blind_temp_checker.ino` for shapes: DS18B20 emits `{ "tempC":..., "tempF":..., "ts":..., "id?":..., "addr?":... }`; IR sketch adds `proximity`.
- Server behavior: `server/server.js` auto-reconnects to `process.env.SERIAL_PORT` with exponential backoff, falls back to simulated readings, and rate-limits per-client broadcasts (`CLIENT_RATE_MS`).
- Calibration flow: `SETCAL <id> <offset> <mult>` over serial and `POST /calibration/:id` (JSON `{offset,mult}`) on server. Server persists calibrations to `BlindTempChecker/server/calibration.json` and attempts to write `SETCAL` back to device.

Useful endpoints and messages:
- HTTP: `/temp` (JSON or ?fmt=cbor), `/health`, `/calibration` (GET/POST)
- WebSocket: server broadcasts objects like `{ type: 'reading', data: { tempC, tempF, ts, id?, addr?, raw? } }`.

Developer workflows (verified):
- Local hardware: upload `BlindTempChecker/arduino/blind_temp_checker.ino` -> set `SERIAL_PORT` -> `cd BlindTempChecker/server && npm install && node server.js`.
- Docker: `docker compose up --build` from repo root; pass your serial device with `--device /dev/ttyACM0` or `-e SERIAL_PORT` in container run.

Testing and helper scripts:
- Unit tests: `BlindTempChecker/server/test/temp.test.js` (run `npm test` from `server/`).
- E2E: Playwright specs in `BlindTempChecker/web/tests/` (run `npm run test:e2e` from `server/`).
- Manual WS tooling: `tools/demo-client.js`, `tools/bench-ws.js` to exercise the WebSocket API.

Project-specific conventions to preserve:
- Keep firmware JSON backward-compatible: add optional fields, don't rename required fields.
- Server tolerates non-JSON serial lines (broadcasts as raw) — avoid breaking that behavior.
- Per-sensor calibration exists on-device (EEPROM) and server-side — when changing calibration storage update both sides.

Files to inspect for common changes:
- Serial/parse/reconnect: `BlindTempChecker/server/server.js`
- Calibration code (device + server): `BlindTempChecker/arduino/blind_temp_checker.ino` and `BlindTempChecker/server/calibration.json`
- UI behavior & accessibility: `BlindTempChecker/web/index.html`
- Docker + compose: `BlindTempChecker/server/Dockerfile`, `docker-compose.yml`

When modifying protocol or adding fields, update firmware -> server -> UI in that order. If you'd like, I can prepare a small PR wiring any new field end-to-end.
```# AI Agent Instructions for Talking Thermometer Project

## Project Architecture

This is an assistive technology project with three main components:
- Arduino/ESP code (`BlindTempChecker/arduino/`) - Reads DS18B20 temperature sensor and outputs JSON over serial
- Node.js server (`BlindTempChecker/server/`) - Bridges serial data to WebSocket for real-time updates
- Web UI (`BlindTempChecker/web/`) - Accessible interface with speech feedback

Key data flow: `DS18B20 sensor -> Arduino (serial JSON) -> Node server -> WebSocket -> Web UI -> Speech`

## Development Patterns

### Serial Communication
- Arduino outputs newline-delimited JSON at 115200 baud
- Server has auto-reconnect with exponential backoff for serial reliability
- Environment variable `SERIAL_PORT` configures device (default: `/dev/ttyACM0`)
```js
// Example serial output format from Arduino
{"tempC": 23.45, "tempF": 74.21, "raw": 3752}
```markdown
# AI agent quick-start: Talking Thermometer (BlindTempChecker)

This repository contains three cooperating parts:
- Firmware (Arduino/ESP): `BlindTempChecker/arduino/` — sensor sketches output newline-delimited JSON over Serial.
- Server (Node.js): `BlindTempChecker/server/` — opens serial, parses JSON, broadcasts via WebSocket, and exposes HTTP endpoints.
- Web UI: `BlindTempChecker/web/` — accessible front-end with speech feedback and keyboard shortcuts.

Key points for agents working on the repo:
- Serial format: newline-terminated JSON at 115200 baud. See `arduino/blind_temp_checker.ino`. DS18B20 sketch emits `{ "tempC", "tempF", "ts", "id?", "addr?" }`. IR sketch adds `proximity` and may omit temp fields.
- Server behavior: `server/server.js` implements auto-reconnect with exponential backoff, simulated fallback (if no serial), and per-client rate-limiting (`CLIENT_RATE_MS`).
- Calibration: `SETCAL <id> <offset> <mult>` over serial; server exposes `POST /calibration/:id` with `{offset,mult}` and persists to `BlindTempChecker/server/calibration.json`.

Important endpoints & messages:
- HTTP: `/temp` (supports `?fmt=cbor`), `/health`, `/calibration` (GET/POST).
- WebSocket: broadcasts `{"type":"reading","data":{...}}` and also `raw`, `info`, `error` messages when appropriate.

Common developer flows (explicit):
- Local hardware: upload Arduino sketch → set `SERIAL_PORT` → `cd BlindTempChecker/server` → `npm install` → `node server.js`.
- Tests: `npm test` (Jest) in `server/`; `npm run test:e2e` runs Playwright for the web UI.
- Docker: `docker compose up --build` from repo root; to pass serial device add `--device /dev/ttyACM0` or set `SERIAL_PORT` env.

Project conventions to preserve (do not change silently):
- Backwards-compatible JSON: add optional fields, avoid renames.
- Server accepts non-JSON serial lines (broadcasts them as `raw`) — the UI and tests rely on this tolerance.
- Calibration exists both on-device (EEPROM) and server-side — keep persistence and SETCAL wiring in sync.

Where to look when making changes:
- Serial + reconnect + fallback: `BlindTempChecker/server/server.js`
- Calibration persistence & HTTP APIs: `BlindTempChecker/server/calibration.json` and `server.js`
- Firmware formats & smoothing: `BlindTempChecker/arduino/blind_temp_checker.ino`
- UI/speech & keyboard shortcuts: `BlindTempChecker/web/index.html`

If you change the serial protocol or add new fields, update firmware → server → web (in that order). Ask if you want a small PR wiring a new field end-to-end.
# AI agent quick-start: Talking Thermometer (BlindTempChecker)

This repository contains three cooperating parts:
- Firmware (Arduino/ESP): `BlindTempChecker/arduino/` — sensor sketches output newline-delimited JSON over Serial.
- Server (Node.js): `BlindTempChecker/server/` — opens serial, parses JSON, broadcasts via WebSocket, and exposes HTTP endpoints.
- Web UI: `BlindTempChecker/web/` — accessible front-end with speech feedback and keyboard shortcuts.

Key points for agents working on the repo:
- Serial format: newline-terminated JSON at 115200 baud. See `arduino/blind_temp_checker.ino`. DS18B20 sketch emits `{ "tempC", "tempF", "ts", "id?", "addr?" }`. IR sketch adds `proximity` and may omit temp fields.
- Server behavior: `server/server.js` implements auto-reconnect with exponential backoff, simulated fallback (if no serial), and per-client rate-limiting (`CLIENT_RATE_MS`).
- Calibration: `SETCAL <id> <offset> <mult>` over serial; server exposes `POST /calibration/:id` with `{offset,mult}` and persists to `BlindTempChecker/server/calibration.json`.

Important endpoints & messages:
- HTTP: `/temp` (supports `?fmt=cbor`), `/health`, `/calibration` (GET/POST).
- WebSocket: broadcasts `{"type":"reading","data":{...}}` and also `raw`, `info`, `error` messages when appropriate.

Common developer flows (explicit):
- Local hardware: upload Arduino sketch → set `SERIAL_PORT` → `cd BlindTempChecker/server` → `npm install` → `node server.js`.
- Tests: `npm test` (Jest) in `server/`; `npm run test:e2e` runs Playwright for the web UI.
- Docker: `docker compose up --build` from repo root; to pass serial device add `--device /dev/ttyACM0` or set `SERIAL_PORT` env.

Project conventions to preserve (do not change silently):
- Backwards-compatible JSON: add optional fields, avoid renames.
- Server accepts non-JSON serial lines (broadcasts them as `raw`) — the UI and tests rely on this tolerance.
- Calibration exists both on-device (EEPROM) and server-side — keep persistence and SETCAL wiring in sync.

Where to look when making changes:
- Serial + reconnect + fallback: `BlindTempChecker/server/server.js`
- Calibration persistence & HTTP APIs: `BlindTempChecker/server/calibration.json` and `server.js`
- Firmware formats & smoothing: `BlindTempChecker/arduino/blind_temp_checker.ino`
- UI/speech & keyboard shortcuts: `BlindTempChecker/web/index.html`

If you change the serial protocol or add new fields, update firmware → server → web (in that order). Ask if you want a small PR wiring a new field end-to-end.
