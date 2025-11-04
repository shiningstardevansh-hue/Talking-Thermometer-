const express = require('express');
const http = require('http');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const WebSocket = require('ws');
const cors = require('cors');

const app = express();
app.use(cors());
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 3000;
// Read serial port from env or try common Linux device. On Windows set SERIAL_PORT env.
const SERIAL_PORT = process.env.SERIAL_PORT || '/dev/ttyACM0';
const BAUD = parseInt(process.env.SERIAL_BAUD || '115200', 10);

let lastReading = { tempC: null, tempF: null, ts: null, raw: null };

// Broadcast helper
function broadcast(obj) {
  const s = JSON.stringify(obj);
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) client.send(s);
  });
}

// Serial handling with auto-reconnect + backoff
const fs = require('fs');
let parser = null;
let serialPort = null;
let connecting = false;
let backoff = 1000;

function tryOpenSerial() {
  if (connecting) return;
  connecting = true;
  try {
    serialPort = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD, autoOpen: false });
    serialPort.open((err) => {
      connecting = false;
      if (err) {
        console.warn('Serial open failed:', err.message);
        backoff = Math.min(backoff * 2, 30000);
        setTimeout(tryOpenSerial, backoff);
        return;
      }
      console.log('Serial opened', SERIAL_PORT);
      backoff = 1000;
      parser = serialPort.pipe(new ReadlineParser({ delimiter: '\n' }));
      parser.on('data', handleSerialLine);
      serialPort.on('close', () => {
        console.warn('Serial closed, will retry');
        parser = null; serialPort = null;
        setTimeout(tryOpenSerial, 2000);
      });
      serialPort.on('error', (e) => { console.error('Serial error', e.message); });
    });
  } catch (e) {
    connecting = false;
    console.warn('Serial create failed', e.message);
    setTimeout(tryOpenSerial, backoff);
    backoff = Math.min(backoff * 2, 30000);
  }
}

function handleSerialLine(line) {
  line = line.trim();
  if (!line) return;
  try {
    const obj = JSON.parse(line);
    if (obj.tempC !== undefined) {
      // sensor-specific reading
      const reading = { tempC: parseFloat(obj.tempC), tempF: parseFloat(obj.tempF || (obj.tempC * 9 / 5 + 32)), ts: obj.ts || Date.now(), raw: obj, id: obj.id, addr: obj.addr };
      lastReading = reading;
      broadcastRateLimited({ type: 'reading', data: reading });
      return;
    }
    if (obj.error) {
      console.warn('Sensor error:', obj.error);
      broadcastRateLimited({ type: 'error', data: obj });
      return;
    }
    broadcastRateLimited({ type: 'info', data: obj });
  } catch (e) {
    lastReading.raw = line;
    broadcastRateLimited({ type: 'raw', data: line });
  }
}

tryOpenSerial();

// fallback simulated data if serial never opened after short delay
setTimeout(() => {
  if (!parser) {
    console.warn('Using simulated data (no serial)');
    setInterval(() => {
      const tC = 20 + Math.sin(Date.now() / 60000) * 5 + (Math.random() - 0.5) * 0.2;
      const tF = tC * 9 / 5 + 32;
      lastReading = { tempC: parseFloat(tC.toFixed(3)), tempF: parseFloat(tF.toFixed(3)), ts: Date.now(), raw: null };
      broadcastRateLimited({ type: 'reading', data: lastReading });
    }, 1000);
  }
}, 1500);

// Calibration persistence
const CAL_FILE = __dirname + '/calibration.json';
let calibrations = {};
try { if (fs.existsSync(CAL_FILE)) calibrations = JSON.parse(fs.readFileSync(CAL_FILE)); } catch (e) { calibrations = {}; }

function saveCalibrations() { fs.writeFileSync(CAL_FILE, JSON.stringify(calibrations, null, 2)); }

// Rate-limited broadcast: per-client throttle and global debounce
const CLIENT_RATE_MS = 200; // 5 msgs/s per client
function broadcastRateLimited(obj) {
  const s = JSON.stringify(obj);
  wss.clients.forEach((client) => {
    if (client.readyState !== WebSocket.OPEN) return;
    const now = Date.now();
    if (!client._lastSent || (now - client._lastSent) >= CLIENT_RATE_MS) {
      client.send(s);
      client._lastSent = now;
    }
  });
}

app.get('/temp', (req, res) => {
  if (lastReading.tempC === null) return res.status(503).send('no_data');
  res.json(lastReading);
});

// simple health endpoint
app.get('/health', (req, res) => res.send({ ok: true }));

// WebSocket connection logging
wss.on('connection', (ws) => {
  // send last reading immediately
  if (lastReading.tempC !== null) ws.send(JSON.stringify({ type: 'reading', data: lastReading }));
  ws.on('message', (m) => {
    // handle simple commands in future
    try { const msg = JSON.parse(m); /* no-op for now */ } catch (e) {}
  });
});

server.listen(PORT, () => console.log(`✅ Server running at http://localhost:${PORT}  (WS enabled)`));

// Calibration endpoints
app.get('/calibration', (req, res) => res.json(calibrations));
app.post('/calibration/:id', express.json(), (req, res) => {
  const id = req.params.id;
  const { offset, mult } = req.body || {};
  if (typeof offset !== 'number' && typeof mult !== 'number') return res.status(400).send('bad body');
  calibrations[id] = { offset: offset || 0, mult: mult || 1 };
  saveCalibrations();
  // If an Arduino is connected, send the calibration command so the device persists it too
  try {
    if (serialPort && serialPort.isOpen) {
      const off = calibrations[id].offset;
      const mu = calibrations[id].mult;
      const cmd = `SETCAL ${id} ${off} ${mu}\n`;
      serialPort.write(cmd);
      console.log('Wrote to serial:', cmd.trim());
    }
  } catch (e) {
    console.warn('Failed to write calibration to serial:', e.message);
  }

  res.json({ ok: true, id, calibration: calibrations[id] });
});
