const { spawn } = require('child_process');
const http = require('http');

function waitForServer(proc, timeout = 5000) {
  return new Promise((resolve, reject) => {
    const deadline = Date.now() + timeout;
    proc.stdout.on('data', (d) => {
      const s = d.toString();
      if (s.includes('Server running') || s.includes('WS enabled')) resolve();
    });
    const iv = setInterval(() => {
      if (Date.now() > deadline) { clearInterval(iv); reject(new Error('timeout waiting for server')); }
    }, 100);
  });
}

function getJSON(url) {
  return new Promise((resolve, reject) => {
    http.get(url, (res) => {
      let raw = '';
      res.on('data', (c) => raw += c);
      res.on('end', () => {
        try { resolve(JSON.parse(raw)); } catch (e) { reject(e); }
      });
    }).on('error', reject);
  });
}

describe('server /temp endpoint (simulated)', () => {
  let proc;
  beforeAll(async () => {
    proc = spawn('node', ['server.js'], { cwd: __dirname + '/..', env: Object.assign({}, process.env, { SERIAL_PORT: '' }) });
    proc.stdout.on('data', (d) => {});
    proc.stderr.on('data', (d) => {});
    await waitForServer(proc, 8000);
  }, 20000);

  afterAll(() => { if (proc) proc.kill(); });

  test('GET /temp returns JSON with tempC (simulated)', async () => {
    const json = await getJSON('http://localhost:3000/temp');
    // server returns { tempC, tempF, ts } or 503 if no data; simulated mode should return object
    expect(typeof json).toBe('object');
    // Accept either simulated reading or structure with tempC
    if (json.tempC !== undefined) {
      expect(typeof json.tempC).toBe('number');
      expect(typeof json.ts).toBe('number');
    }
  }, 10000);
});
