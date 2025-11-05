const WebSocket = require('ws');
const url = process.env.WS_URL || 'ws://localhost:3000';
const ws = new WebSocket(url);
ws.on('open', () => console.log('connected to', url));
ws.on('message', (m) => {
  try { const msg = JSON.parse(m); console.log('MSG', msg.type, msg.data); } catch (e) { console.log('RAW', m.toString()); }
});
ws.on('close', () => console.log('closed'));
ws.on('error', (e) => console.error('err', e.message));
