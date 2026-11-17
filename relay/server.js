#!/usr/bin/env node
/*
 * relay/server.js - thin relay between the MASM program and the Guard / Controller apps.
 *
 *   DOSBox (traffic.exe on COM1)  <--TCP-->  this relay  <--WebSocket-->  apps
 *
 * The relay holds NO traffic logic. It
 *   - connects to DOSBox's COM1 TCP port and reconnects forever,
 *   - turns the MASM "S ..." lines into JSON and pushes them to every app at once,
 *   - lets only controllers (with the PIN) send commands, and passes only a fixed set of
 *     command shapes to the UART; the MASM program validates them again,
 *   - serves the two apps as static files.
 *
 * Usage:  node server.js [--port 8080] [--dos 127.0.0.1:5000] [--pin 1234]
 *         (or env PORT, DOS_ADDR, CONTROL_PIN)
 */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const net = require('net');
const os = require('os');
const { performance } = require('perf_hooks');
const { WebSocketServer } = require('ws');

// ---------------------------------------------------------------- config
function arg(name, env, dflt) {
  const i = process.argv.indexOf('--' + name);
  if (i > 0 && process.argv[i + 1]) return process.argv[i + 1];
  return process.env[env] || dflt;
}
const HTTP_PORT = parseInt(arg('port', 'PORT', '8080'), 10);
const [DOS_HOST, DOS_PORT_S] = arg('dos', 'DOS_ADDR', '127.0.0.1:5000').split(':');
const DOS_PORT = parseInt(DOS_PORT_S, 10);
const PIN = String(arg('pin', 'CONTROL_PIN', '1234'));
const APPS_DIR = path.resolve(arg('apps', 'APPS_DIR', path.join(__dirname, '..', 'apps')));
const STALE_MS = 2500;          // no line from MASM for this long -> link shown as lost
const PROBE_MS = 1000;          // relay <-> MASM round-trip probe
const MAX_CMDS_PER_SEC = 25;

const nowMs = () => performance.timeOrigin + performance.now();

// ---------------------------------------------------------------- link to MASM (COM1 over TCP)
let sock = null;
let tcpUp = false;
let stale = true;
let lastLineAt = 0;
let lastState = null;           // last parsed S message (sent to apps that connect later)
let coreRtt = null;             // ms, relay -> MASM -> relay
let rxText = '';
let idCounter = 0;
// COM1 has no flow control and the 8250/16550 receive buffer is tiny (DOSBox: 16 bytes), so the relay
// is strictly stop-and-wait: one line (+ its "? id" ack request) in flight, the next goes out when
// MASM echoes that id back in its S line. A UART with ~12 bytes in flight can never overrun.
const outq = [];                // waiting: { kind: 'probe'|'cmd', cmd, client, cid }
let busy = null;                // in flight: the item above plus { id, t }
let busyTimer = null;
const ACK_TIMEOUT_MS = 600;
const MAX_QUEUE = 40;
const nextId = () => (idCounter = (idCounter % 65535) + 1);

function pump() {
  if (busy || !outq.length || !sock || !tcpUp) return;
  busy = outq.shift();
  busy.id = nextId();
  busy.t = nowMs();
  sock.write(busy.kind === 'cmd' ? `${busy.cmd}\n? ${busy.id}\n` : `? ${busy.id}\n`);
  busyTimer = setTimeout(() => {                       // no echo: give up on this one, go on
    const b = busy; busy = null;
    if (b && b.kind === 'cmd' && b.client.ws.readyState === 1) send(b.client.ws, { t: 'E', code: 'timeout', cid: b.cid });
    pump();
  }, ACK_TIMEOUT_MS);
}
function acked(t) {
  clearTimeout(busyTimer);
  const b = busy; busy = null;
  if (b.kind === 'probe') coreRtt = Math.round((t - b.t) * 100) / 100;
  else if (b.client.ws.readyState === 1) send(b.client.ws, { t: 'ack', cid: b.cid, ms: t - b.t, ts: t });
  pump();
}

const linkOk = () => tcpUp && !stale;

function connectDos() {
  const s = net.createConnection({ host: DOS_HOST, port: DOS_PORT });
  s.setNoDelay(true);                       // never hold small packets back
  let done = false;
  const lost = (why) => {
    if (done) return;
    done = true;
    if (sock === s) sock = null;
    if (tcpUp) console.log(`[dos] link closed (${why})`);
    tcpUp = false;
    publishLink('tcp-down');
    setTimeout(connectDos, 500);
  };
  s.on('connect', () => {
    sock = s;
    tcpUp = true;
    stale = true;
    rxText = '';
    outq.length = 0; busy = null; clearTimeout(busyTimer);
    console.log(`[dos] connected to ${DOS_HOST}:${DOS_PORT}`);
    s.write('?\n');                         // ask the MASM program for a snapshot right away
  });
  s.on('data', onDosData);
  s.on('error', (e) => lost(e.code || e.message));
  s.on('close', () => lost('closed'));
}

/** Bytes from the UART. Telnet-style 0xFF escapes (DOSBox without transparent:1) are dropped. */
function onDosData(buf) {
  for (let i = 0; i < buf.length; i++) {
    const b = buf[i];
    if (b === 0xff) { i++; continue; }
    if (b === 0x0a || b === 0x0d) {
      if (rxText) { handleLine(rxText); rxText = ''; }
    } else if (b >= 0x20 && b < 0x7f && rxText.length < 200) {
      rxText += String.fromCharCode(b);
    }
  }
}

function handleLine(line) {
  const t = nowMs();
  lastLineAt = t;
  if (line[0] === 'S') {
    const p = line.split(' ');
    if (p.length !== 12) return;
    const n = p.map((x, i) => (i === 1 || i === 5 ? x : Number(x)));
    if (!'GYRO'.includes(n[1]) || n.slice(2).some((x, i) => i !== 3 && !Number.isFinite(x))) return;
    const ack = n[8];
    const wasStale = stale;
    stale = false;
    lastState = {
      t: 'S', st: n[1], dir: n[2], rem: n[3], dur: n[4], mode: n[5], pause: n[6],
      seq: n[7], ack, cfg: [n[9], n[10], n[11]], ts: t, rtt: coreRtt,
    };
    broadcast(lastState);                   // state first ...
    if (busy && busy.id === ack) acked(t);  // ... then the ack, so an app never sees 'applied' before the change
    if (wasStale) publishLink('ok');
  } else if (line[0] === 'E') {
    broadcastTo('controller', { t: 'E', code: line.slice(2).trim(), ts: t });
  }
}

function publishLink(reason) {
  broadcast({ t: 'L', link: linkOk(), reason: linkOk() ? 'ok' : reason, ts: nowMs() });
}

setInterval(() => {                         // watchdog: the MASM program sends a line every second
  if (tcpUp && !stale && nowMs() - lastLineAt > STALE_MS) {
    stale = true;
    publishLink('no-heartbeat');
  }
}, 250);

setInterval(() => {                         // measure relay <-> MASM round trip (only when idle)
  if (!sock || !tcpUp || busy || outq.length) return;
  outq.push({ kind: 'probe' });
  pump();
}, PROBE_MS);

// ---------------------------------------------------------------- web server (static apps)
const MIME = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.woff2': 'font/woff2', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json',
  '.webmanifest': 'application/manifest+json', '.ico': 'image/x-icon',
};

function lanUrls() {
  const out = [];
  for (const list of Object.values(os.networkInterfaces()))
    for (const i of list || []) if (i.family === 'IPv4' && !i.internal) out.push(i.address);
  return out;
}

function landing() {
  const ips = lanUrls();
  const hosts = ['localhost', ...ips];
  const rows = hosts.map((h) =>
    `<li><b>${h}</b> &nbsp; <a href="http://${h}:${HTTP_PORT}/guard/">guard</a> &nbsp; <a href="http://${h}:${HTTP_PORT}/controller/">controller</a></li>`).join('');
  return `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Traffic signal</title><link rel="stylesheet" href="/common/base.css">
<body class="landing"><main><h1>Traffic signal</h1>
<p>Open the guard app on the phone or tablet at the junction, and the controller app at the console.</p>
<ul>${rows}</ul><p class="dim">Controller PIN is set when the relay starts.</p></main></body>`;
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  let p = decodeURIComponent(url.pathname);
  if (p === '/') { res.writeHead(200, { 'Content-Type': MIME['.html'], 'Cache-Control': 'no-cache' }); return res.end(landing()); }
  if (p === '/status') {
    res.writeHead(200, { 'Content-Type': MIME['.json'], 'Cache-Control': 'no-cache' });
    return res.end(JSON.stringify({ tcp: tcpUp, link: linkOk(), coreRtt, state: lastState, guards: count('guard'), controllers: count('controller') }));
  }
  if (p.endsWith('/')) p += 'index.html';
  const file = path.normalize(path.join(APPS_DIR, p));
  if (!file.startsWith(APPS_DIR + path.sep)) { res.writeHead(403); return res.end('forbidden'); }
  fs.readFile(file, (err, data) => {
    if (err) {
      if (!path.extname(p) && fs.existsSync(path.join(APPS_DIR, p, 'index.html'))) { res.writeHead(301, { Location: p + '/' }); return res.end(); }
      res.writeHead(404, { 'Content-Type': 'text/plain' }); return res.end('not found');
    }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream', 'Cache-Control': 'no-cache' });
    res.end(data);
  });
});
server.on('connection', (s) => s.setNoDelay(true));

// ---------------------------------------------------------------- WebSocket hub
const wss = new WebSocketServer({ server, path: '/ws', perMessageDeflate: false });
const clients = new Set();                  // { ws, role, cmdTimes }

function count(role) { let n = 0; for (const c of clients) if (c.role === role) n++; return n; }
function send(ws, obj) { if (ws.readyState === 1) ws.send(typeof obj === 'string' ? obj : JSON.stringify(obj)); }
function broadcast(obj) { const s = JSON.stringify(obj); for (const c of clients) send(c.ws, s); }
function broadcastTo(role, obj) { const s = JSON.stringify(obj); for (const c of clients) if (c.role === role) send(c.ws, s); }
function presence() { broadcast({ t: 'P', guards: count('guard'), controllers: count('controller') }); }

const CMD_RE = /^(?:[GYRXZAM]|P[01]|D[0-7]|T \d{1,2} \d{1,2} \d{1,2})$/;

wss.on('connection', (ws, req) => {
  ws._socket.setNoDelay(true);
  const q = new URL(req.url, 'http://x').searchParams;
  const role = q.get('role') === 'controller' ? 'controller' : 'guard';
  if (role === 'controller' && q.get('pin') !== PIN) {
    send(ws, { t: 'auth', ok: false });
    return ws.close(4001, 'bad pin');
  }
  const c = { ws, role, cmdTimes: [] };
  clients.add(c);
  ws.isAlive = true;
  ws.on('pong', () => { ws.isAlive = true; });
  send(ws, { t: 'hello', role, ts: nowMs(), link: linkOk(), guards: count('guard') , controllers: count('controller'), coreRtt });
  if (lastState) send(ws, lastState);
  presence();

  ws.on('message', (raw) => {
    let m;
    try { m = JSON.parse(raw); } catch { return; }
    if (m.t === 'ping') return send(ws, { t: 'pong', c: m.c, ts: nowMs(), rtt: coreRtt });
    if (m.t !== 'cmd') return;
    if (role !== 'controller') return send(ws, { t: 'E', code: 'denied', cid: m.cid });
    const t = Date.now();
    c.cmdTimes = c.cmdTimes.filter((x) => t - x < 1000);
    if (c.cmdTimes.length >= MAX_CMDS_PER_SEC) return send(ws, { t: 'E', code: 'too-fast', cid: m.cid });
    c.cmdTimes.push(t);
    const cmd = String(m.cmd || '').toUpperCase();
    const cid = Number(m.cid);
    if (!CMD_RE.test(cmd) || !Number.isInteger(cid) || cid < 1 || cid > 59999)
      return send(ws, { t: 'E', code: 'bad-command', cid: m.cid });
    if (!sock || !tcpUp) return send(ws, { t: 'E', code: 'link-down', cid });
    if (outq.length >= MAX_QUEUE) return send(ws, { t: 'E', code: 'too-fast', cid });
    outq.push({ kind: 'cmd', cmd, client: c, cid });
    pump();
  });
  ws.on('close', () => { clients.delete(c); presence(); });
  ws.on('error', () => {});
});

setInterval(() => {                         // drop dead sockets (phone went out of range)
  for (const c of clients) {
    if (!c.ws.isAlive) { c.ws.terminate(); continue; }
    c.ws.isAlive = false;
    c.ws.ping();
  }
}, 10000);

// ---------------------------------------------------------------- start
server.listen(HTTP_PORT, '0.0.0.0', () => {
  console.log(`Traffic relay on port ${HTTP_PORT}   (controller PIN ${PIN})`);
  for (const h of ['localhost', ...lanUrls()]) {
    console.log(`  guard       http://${h}:${HTTP_PORT}/guard/`);
    console.log(`  controller  http://${h}:${HTTP_PORT}/controller/`);
  }
  console.log(`Waiting for DOSBox COM1 at ${DOS_HOST}:${DOS_PORT} ...`);
  connectDos();
});
