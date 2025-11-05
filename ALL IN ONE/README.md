# Blind Temperature Checker

This repository contains a small project to read DS18B20 temperature sensors from an Arduino (or ESP) and serve readings to a web UI for blind or low-vision users. The server reads newline-delimited JSON from the Arduino over serial and broadcasts to WebSocket clients for low-latency updates.

Contents
- BlindTempChecker/arduino/blind_temp_checker.ino — Arduino sketch (DS18B20, 12-bit, median+EMA smoothing, JSON output)
- BlindTempChecker/server/server.js — Node server (serial + WebSocket, simulated fallback)
- BlindTempChecker/server/package.json — dependencies
- BlindTempChecker/web/index.html — Web UI (accessible, speech feedback)
- BlindTempChecker.zip — downloadable package of the project

Quick start (Linux)
1. Open VS Code and the project folder (or extract the ZIP).
2. Connect your Arduino with DS18B20 wired to D2 and a 4.7kΩ pull-up between DATA and VCC.
   - Use normal 3.3V/5V power (avoid parasitic mode unless you know how).
3. Upload the sketch: Open `BlindTempChecker/arduino/blind_temp_checker.ino` in the Arduino extension or Arduino IDE and upload to your board.
4. Start the server:

```bash
cd BlindTempChecker/server
# set your serial device, e.g. /dev/ttyACM0 or /dev/ttyUSB0
export SERIAL_PORT=/dev/ttyACM0
npm install
node server.js
```

5. Open the web UI:
- Use Live Server in VS Code or open `BlindTempChecker/web/index.html` in your browser.
- The page connects to the server via WebSocket and will announce temperature readings.

Docker (optional): build and run the server in Docker. Useful if you want to try the final product without installing npm deps locally.

Build and run with docker-compose (maps your serial device through to container):

```bash
# from repository root
export SERIAL_PORT=/dev/ttyACM0   # change to your serial device
docker compose up --build
```

Or build and run the server container directly:

```bash
cd BlindTempChecker/server
docker build -t blind-temp-server .
docker run --rm -p 3000:3000 -e SERIAL_PORT=/dev/ttyACM0 --device /dev/ttyACM0 blind-temp-server
```

Notes & tips
- Serial port: On Linux, typical devices are `/dev/ttyACM0` or `/dev/ttyUSB0`. On Windows use `COM3` style names.
- If the server cannot open your serial port it will generate simulated readings so you can test the UI.
- Wiring: DS18B20 DATA -> D2, VCC -> 5V (or 3.3V for 3.3V boards), GND -> GND. Add a 4.7kΩ resistor between DATA and VCC.
- Resolution vs speed: 12-bit resolution gives 0.0625°C precision but conversion takes ~750ms. The sketch uses a 1s poll and smoothing (median+EMA) to balance accuracy and responsiveness.

Next recommended steps
- Add per-sensor calibration offsets and persist them in EEPROM (todo).
- Add multi-sensor detection and per-sensor IDs.
- Add Dockerfile and CI for automated deploy and testing.

License
- Add a LICENSE file if you'd like to publish this repo publicly.

If you'd like, I can now create `.gitignore` and push these docs to the repository for you.