// server.js
// Envia comando "instant" para o ESP32 via HTTP POST
// Instale dependência: npm install axios otplib

const axios = require('axios');

// ---------- Configurações ----------
const hostAPI = 'https://esp-server.neurelix.com.br/api/cmd';
const quantidade = '2/4';
const nomeUsuario = 'Giuseph';
// === Token "crip" v2 ===
const K1 = 73856093n;   // primos grandes (mesmos do ESP32)
const K2 = 83492791n;
// -----------------------------------

// Fuso horário Brasil
process.env.TZ = 'America/Sao_Paulo';

function gerarToken() {
  const timestamp = Math.floor(Date.now() / 1000);
  const minUTC    = Math.floor(timestamp / 60) * 60;     // minuto arredondado

  let tok = ( (BigInt(minUTC) * K1) ^ K2 ) % 1000000n;   // 0‒999999
  const tokenStr = tok.toString().padStart(6,'0');

  console.log('Timestamp', timestamp, ' minUTC', minUTC, ' token', tokenStr);
  return tokenStr;
}

const tokenAtual = gerarToken();
const comando    = `instant|${quantidade}|${tokenAtual}|${nomeUsuario}`;

async function enviarComando() {
  try {
    console.log('Enviando:', comando);
    const payload = 'PdeNG:' + comando;
    const res = await axios.post(hostAPI, payload, {
      headers: { 'Content-Type':'application/x-www-form-urlencoded' }
    });
    console.log('Resposta:', res.data);
  } catch(err) {
    console.error('ERRO:', err.message);
  }
}

enviarComando(); 