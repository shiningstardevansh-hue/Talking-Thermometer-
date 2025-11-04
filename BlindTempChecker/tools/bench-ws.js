// Simple WebSocket benchmark: open many clients and count messages for a short period
const WebSocket = require('ws');
const url = process.env.WS_URL || 'ws://localhost:3000';
const CLIENTS = parseInt(process.env.CLIENTS || '50', 10);
const DURATION = parseInt(process.env.DURATION || '10000', 10);

let totalMsgs = 0;
const clients = [];
for (let i=0;i<CLIENTS;i++){
  const ws = new WebSocket(url);
  ws.on('open', ()=>{});
  ws.on('message', ()=>{ totalMsgs++; });
  clients.push(ws);
}

console.log(`Started ${CLIENTS} clients, measuring for ${DURATION}ms`);
setTimeout(()=>{
  console.log(`Total messages received across clients: ${totalMsgs}`);
  clients.forEach(c=>c.close());
  process.exit(0);
}, DURATION);
