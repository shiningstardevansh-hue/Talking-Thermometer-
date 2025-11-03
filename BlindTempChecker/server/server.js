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

// Try to open serial port. If it fails, fall back to simulated data.
let parser = null;
try {
  const serial = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD, autoOpen: true });
  parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  console.log('Opening serial', SERIAL_PORT, 'at', BAUD);
  parser.on('data', (line) => {
    line = line.trim();
    try {
      const obj = JSON.parse(line);
      if (obj.tempC !== undefined) {
        lastReading = { tempC: parseFloat(obj.tempC), tempF: parseFloat(obj.tempF || (obj.tempC * 9 / 5 + 32)), ts: obj.ts || Date.now(), raw: obj };
        broadcast({ type: 'reading', data: lastReading });
      } else if (obj.error) {
        console.warn('Sensor error:', obj.error);
        broadcast({ type: 'error', data: obj });
      } else {
        // other info
        broadcast({ type: 'info', data: obj });
      }
    } catch (e) {
      // not JSON - keep as raw string
      lastReading.raw = line;
      broadcast({ type: 'raw', data: line });
    }
  });
  serial.on('error', (err) => {
    console.error('Serial error:', err.message);
  });
} catch (err) {
  console.warn('Could not open serial port', SERIAL_PORT, '- falling back to simulated data. Error:', err.message);
}

// Simulate data if no parser available
if (!parser) {
  setInterval(() => {
    const tC = 20 + Math.sin(Date.now() / 60000) * 5 + (Math.random() - 0.5) * 0.2;
    const tF = tC * 9 / 5 + 32;
    lastReading = { tempC: parseFloat(tC.toFixed(3)), tempF: parseFloat(tF.toFixed(3)), ts: Date.now(), raw: null };
    broadcast({ type: 'reading', data: lastReading });
  }, 1000);
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
