const fetch = require('node-fetch');
const cbor = require('cbor');
const url = process.env.SERVER_URL || 'http://localhost:3000/temp?fmt=cbor';

(async () => {
  try {
    const res = await fetch(url);
    const buf = await res.arrayBuffer();
    const decoded = cbor.decodeFirstSync(Buffer.from(buf));
    console.log('Decoded CBOR:', decoded);
  } catch (e) {
    console.error('Error fetching/decoding CBOR:', e.message);
  }
})();
