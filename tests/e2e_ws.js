// End-to-end test: relay + MASM (or the stand-in) + WebSocket clients. Run with the relay and the
// DOS side already started:   node tests/e2e_ws.js [host:port] [pin]
'use strict';
const path = require('path');
const WebSocket = require(path.join(__dirname, '..', 'relay', 'node_modules', 'ws'));
const { performance } = require('perf_hooks');
const addr = process.argv[2] || 'localhost:8080', pin = process.argv[3] || '1234';
let fails = 0;
const check = (c, m) => { console.log((c ? 'PASS ' : 'FAIL ') + m); if (!c) fails++; };
const lastS = (cl) => [...cl.msgs].reverse().find((m) => m.t === 'S');
const ackOf = (cl, cid) => cl.wait((m) => m.t === 'ack' && m.cid === cid);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function client(role, p = pin) {
  const ws = new WebSocket(`ws://${addr}/ws?role=${role}` + (role === 'controller' ? `&pin=${p}` : ''), { perMessageDeflate: false });
  const c = { ws, msgs: [], waiters: [], closed: null };
  ws.on('message', (d) => { const m = JSON.parse(d); m._at = performance.now(); c.msgs.push(m); c.waiters = c.waiters.filter((w) => !w(m)); });
  ws.on('close', (code) => { c.closed = code; });
  c.open = new Promise((r) => ws.on('open', r));
  c.wait = (pred, ms = 3000) => new Promise((res, rej) => {
    const hit = c.msgs.find(pred); if (hit) return res(hit);
    const t = setTimeout(() => rej(new Error('timeout')), ms);
    c.waiters.push((m) => (pred(m) ? (clearTimeout(t), res(m), true) : false));
  });
  c.cmd = (cmd, cid) => ws.send(JSON.stringify({ t: 'cmd', cmd, cid }));
  return c;
}

(async () => {
  const g = client('guard'), c = client('controller');
  await Promise.all([g.open, c.open]);
  const s0g = await g.wait((m) => m.t === 'S'), s0c = await c.wait((m) => m.t === 'S');
  check(s0g.st && s0c.st && Array.isArray(s0g.cfg), `both apps get a state snapshot on connect (${s0g.st} ${s0g.rem}s)`);

  // wrong PIN, guard cannot command
  const bad = client('controller', '0000');
  await bad.wait((m) => m.t === 'auth'); await sleep(100);
  check(bad.closed === 4001, 'wrong PIN is refused and the socket closed (4001)');
  g.cmd('R', 5);
  const den = await g.wait((m) => m.t === 'E' && m.code === 'denied');
  check(!!den, 'guard connection cannot send commands (denied)');

  // command: controller -> relay -> MASM -> both apps
  const t0 = performance.now();
  c.cmd('R', 101);
  await ackOf(c, 101);
  const lat = performance.now() - t0;
  const ackC = lastS(c);
  const gR = await g.wait((m) => m.t === 'S' && m.seq >= ackC.seq);
  check(ackC.st === 'R' && gR.st === 'R', 'RED command reaches MASM, both apps see RED');
  check(ackC.rem === ackC.dur, 'MASM restarted the phase timer');
  console.log(`   controller -> MASM -> controller confirmation: ${lat.toFixed(1)} ms; guard saw it ${(gR._at - t0).toFixed(1)} ms after the click; guard-vs-controller skew ${Math.abs(gR._at - ackC._at).toFixed(2)} ms`);

  // full command set
  const tests = [['G', (s) => s.st === 'G'], ['Y', (s) => s.st === 'Y'], ['D5', (s) => s.dir === 5], ['D0', (s) => s.dir === 0],
    ['M', (s) => s.mode === 'M'], ['A', (s) => s.mode === 'A'], ['P1', (s) => s.pause === 1], ['P0', (s) => s.pause === 0],
    ['T 12 4 9', (s) => s.cfg.join() === '12,4,9'], ['T 10 3 10', (s) => s.cfg.join() === '10,3,10'], ['X', (s) => s.st === 'O'], ['Z', (s) => s.st === 'G' && s.rem === s.dur]];
  let id = 200;
  for (const [cmd, pred] of tests) {
    const cid = ++id; c.cmd(cmd, cid);
    try { await ackOf(c, cid); const s = lastS(c); const sg = await g.wait((m) => m.t === 'S' && m.seq >= s.seq); check(pred(s) && pred(sg), `command "${cmd}" applied, both apps agree`); }
    catch (e) { check(false, `command "${cmd}" timed out`); }
  }

  // relay and MASM reject bad input
  c.cmd('T 0 3 10', 301);
  const e2 = await c.wait((m) => m.t === 'E' && m.code === '2').catch(() => null);
  check(!!e2, 'MASM rejects T 0 3 10 with E 2 (forwarded to the controller)');
  c.cmd('HELLO', 302);
  const e3 = await c.wait((m) => m.t === 'E' && m.code === 'bad-command' && m.cid === 302).catch(() => null);
  check(!!e3, 'relay refuses an unknown command before it reaches the UART');
  c.cmd('G\nX', 303);
  const e4 = await c.wait((m) => m.t === 'E' && m.cid === 303).catch(() => null);
  check(!!e4, 'newline injection is refused');
  const ro = g.msgs.filter((m) => m.t === 'E' && m.code !== 'denied');
  check(ro.length === 0, 'guards never receive controller error traffic');

  // heartbeat + presence
  await sleep(2200);
  const ss = c.msgs.filter((m) => m.t === 'S' && m.ts > s0c.ts);
  check(ss.length >= 3, `S updates keep flowing (${ss.length} in the last few seconds)`);
  const pr = g.msgs.filter((m) => m.t === 'P').pop();
  check(pr && pr.guards === 1 && pr.controllers === 1, 'presence counts: 1 guard, 1 controller');

  // burst: ordering is preserved
  const seqs = []; let last = 0;
  for (let i = 0; i < 10; i++) c.cmd(i % 2 ? 'Y' : 'R', 400 + i);
  await ackOf(c, 409).catch(() => {});
  const acks = c.msgs.filter((m) => m.t === 'ack' && m.cid >= 400 && m.cid <= 409);
  check(acks.length === 10, `burst of 10 commands: all 10 confirmed (${acks.length}), none lost to coalescing or probes`);
  check(lastS(c).st === 'Y', 'burst: final state equals the last command (Y)');
  check(!c.msgs.some((m) => m.t === 'E' && m.code === 'too-fast'), 'a 10-command burst is within the relay rate limit');

  // latency distribution
  await sleep(1100);
  const lats = [];
  for (let i = 0; i < 30; i++) { const cid = 1000 + i, t = performance.now(); c.cmd(i % 2 ? 'G' : 'R', cid); await ackOf(c, cid); lats.push(performance.now() - t); await sleep(60); }
  lats.sort((a, b) => a - b);
  console.log(`   latency over 30 commands: min ${lats[0].toFixed(1)}  median ${lats[15].toFixed(1)}  p95 ${lats[28].toFixed(1)}  max ${lats[29].toFixed(1)} ms`);

  console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
  process.exit(fails ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(1); });
