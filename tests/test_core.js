// Node tests for the JavaScript engine, hardware backends, view transform and renderer.
// Run: node tests/test_core.js
const T = Object.assign({}, require('../platform/desktop/js/core.js'), require('../platform/desktop/js/hardware.js'),
                        require('../platform/desktop/js/renderer.js'));
const { SignalController } = require('../platform/desktop/js/controller.js');
const { LightState: S, Direction: D } = T;

let run = 0, failed = 0;
const ok = (c, msg) => { run++; if (!c) { failed++; console.error('FAIL ' + msg); } };
const near = (a, b, msg) => ok(Math.abs(a - b) < 1e-6, `${msg} (${a} != ${b})`);
const fresh = () => { const e = new T.TrafficEngine(); e.initialize(); return e; };

// --- state machine (no graphics, no hardware) ---
{
  const e = fresh();
  ok(e.getState() === S.GREEN, 'starts GREEN'); near(e.getRemainingTime(), 10, 'green 10 s');
  e.update(4); near(e.getRemainingTime(), 6, 'remaining 6');
  e.update(6); ok(e.getState() === S.YELLOW, 'GREEN -> YELLOW'); near(e.getRemainingTime(), 3, 'yellow 3 s');
  e.update(3); ok(e.getState() === S.RED, 'YELLOW -> RED'); near(e.getRemainingTime(), 10, 'red 10 s');
  e.update(10); ok(e.getState() === S.GREEN, 'RED -> GREEN'); near(e.snapshot().duration, 10, 'duration');
}
{
  const e = fresh();
  e.update(11.5); ok(e.getState() === S.YELLOW, 'spans phases'); near(e.getRemainingTime(), 1.5, 'carry');
  e.update(1.5 + 10 + 2); ok(e.getState() === S.GREEN, 'multi-phase'); near(e.getRemainingTime(), 8, 'carry 2');
  e.update(1e9); ok(e.getRemainingTime() > 0 && e.getRemainingTime() <= 10, 'huge delta');
  const before = e.getRemainingTime(); e.update(-5); e.update(NaN); e.update('x'); near(e.getRemainingTime(), before, 'bad deltas ignored');
}
{
  const e = fresh();
  for (let want = 10; want >= 1; want--) { ok(Math.ceil(e.getRemainingTime() - 1e-9) === want, `countdown ${want}`); e.update(1); }
  ok(e.getState() === S.YELLOW, 'after 10 s -> YELLOW');
}
// --- manual / pause / direction ---
{
  const e = fresh(); e.update(3);
  ok(e.setState(S.RED) && e.getState() === S.RED, 'manual RED'); near(e.getRemainingTime(), 10, 'timer restarts');
  ok(!e.setState(9) && !e.setState(-1) && !e.setState('RED') && e.getState() === S.RED, 'invalid state rejected');
  e.setPaused(true); e.update(5); near(e.getRemainingTime(), 10, 'paused');
  e.setPaused(false); e.setMode('MANUAL'); e.update(5); near(e.getRemainingTime(), 10, 'manual mode holds');
  e.setMode('AUTO'); e.update(10); ok(e.getState() === S.GREEN, 'cycle continues after manual change');
}
{
  const e = fresh();
  ok(e.getDirection() === D.STRAIGHT, 'default direction');
  ok(e.setDirection(D.LEFT) && e.getState() === S.GREEN, 'direction independent of colour');
  e.update(10); ok(e.getState() === S.YELLOW && e.getDirection() === D.LEFT, 'direction survives phase change');
  ok(!e.setDirection(8) && !e.setDirection(-1) && !e.setDirection(1.5) && e.getDirection() === D.LEFT, 'invalid direction rejected');
  ok(T.nextDirection(D.ALL) === D.NONE && T.parseDirection('straight_left') === D.STRAIGHT_LEFT && T.parseDirection('x') === null, 'direction helpers');
}
// --- reset / all off / hardware ---
{
  const hw = new T.SimulatedHardware(); hw.initialize();
  const e = fresh(); e.setHardware(hw); e.update(12); e.setDirection(D.RIGHT);
  ok(hw.yellow && hw.directionMask === D.RIGHT, 'hardware follows engine');
  e.allOff(); ok(e.getState() === S.OFF && !hw.red && !hw.yellow && !hw.green && hw.directionMask === 0, 'all off');
  e.update(100); ok(e.getState() === S.OFF, 'OFF never advances');
  e.reset(); ok(e.getState() === S.GREEN && hw.green, 'reset'); near(e.getRemainingTime(), 10, 'reset timer');
}
{
  const e = fresh(), snap = e.snapshot();
  let threw = false; try { 'use strict'; snap.state = S.RED; } catch (_) { threw = true; }
  ok(e.getState() === S.GREEN && Object.isFrozen(snap), 'snapshot is read-only');
}
// --- configuration ---
{
  ok(T.validateConfig(T.defaultConfig()) === '', 'default config valid');
  const bad = (patch) => T.validateConfig(Object.assign(T.defaultConfig(), patch)) !== '';
  ok(bad({ greenDuration: 0 }) && bad({ yellowDuration: -2 }) && bad({ redDuration: NaN }) && bad({ zoom: 9 }) && bad({ direction: 12 }) && bad({ mode: 'X' }), 'invalid configs rejected');
  const e = new T.TrafficEngine({ greenDuration: -1 }); const r = e.initialize();
  ok(!r.ok && r.error && e.config.greenDuration === 10 && e.getState() === S.GREEN, 'invalid config falls back to defaults');
  const e2 = fresh();
  ok(e2.setDuration(S.GREEN, 20) && !e2.setDuration(S.GREEN, 0) && !e2.setDuration(S.OFF, 5), 'setDuration validates');
  e2.setState(S.GREEN); near(e2.getRemainingTime(), 20, 'new duration used on entry');
  const p = T.parseConfig('green = 15\nyellow=4 # c\nred = 12\nzoom = 1.5\ndirection = left_right\nmode = manual\n');
  ok(p.ok && p.config.greenDuration === 15 && p.config.zoom === 1.5 && p.config.direction === D.LEFT_RIGHT && p.config.mode === 'MANUAL', 'parseConfig ok');
  ok(!T.parseConfig('green = abc').ok && !T.parseConfig('speed = 3').ok && !T.parseConfig('green = 0').ok && !T.parseConfig('nonsense').ok && !T.parseConfig('green =').ok, 'parseConfig errors');
}
// --- zoom ---
{
  const v = new T.ViewTransform();
  near(v.scale, 1, 'default zoom'); v.zoomIn(); near(v.scale, 1.25, 'zoom in'); v.zoomOut(); v.zoomOut(); near(v.scale, 0.75, 'zoom out');
  for (let i = 0; i < 50; i++) v.zoomOut(); near(v.scale, 0.5, 'min zoom');
  for (let i = 0; i < 50; i++) v.zoomIn(); near(v.scale, 3, 'max zoom');
  v.resetZoom(); near(v.scale, 1, 'reset zoom');
  ok(!v.setScale(0.1) && !v.setScale(10) && !v.setScale(NaN) && v.scale === 1, 'invalid zoom rejected');
  v.setOrigin(100, 50); v.setScale(2); near(v.sx(10), 120, 'sx'); near(v.sy(-5), 40, 'sy'); near(v.len(7), 14, 'len');
}
// --- renderer: everything scales with zoom (recording canvas, no browser needed) ---
function record(zoom, state) {
  const calls = [];
  const ctx = new Proxy({}, { get: (_, k) => (k === 'createRadialGradient' ? () => ({ addColorStop() {} }) : (...a) => calls.push([k, ...a])), set: () => true });
  const r = new T.CanvasRenderer(ctx), t = new T.ViewTransform(zoom), view = new T.TrafficLightView(r, t);
  const e = fresh(); if (state) { state(e); }
  view.renderFrame(e.snapshot(), 800, 600);
  return calls;
}
{
  const arcs = (c) => c.filter((x) => x[0] === 'arc');
  const a1 = arcs(record(1)), a2 = arcs(record(2));
  ok(a1.length === a2.length && a1.length >= 7, 'same lamp circles at any zoom');
  near(a2[0][3] / a1[0][3], 2, 'lamp radius doubles with zoom 2x');
  near((a2[2][1] - 400) / (a1[2][1] - 400) || 1, 1, 'lamps stay centred');
  const polys = (c) => c.filter((x) => x[0] === 'closePath').length;
  ok(polys(record(1, (e) => e.setDirection(D.NONE))) === 0, 'no arrows for Direction.NONE');
  ok(polys(record(1, (e) => e.setDirection(D.ALL))) === 3, 'three arrows for ALL');
  ok(polys(record(1, (e) => e.setDirection(D.LEFT_RIGHT))) === 2, 'two arrows for LEFT_RIGHT');
  ok(polys(record(1, (e) => e.allOff())) === 0, 'no arrows when all off');
  const texts = record(1.5).filter((x) => x[0] === 'fillText').map((x) => x[1]);
  ok(texts.includes('10') && texts.includes('GREEN'), 'countdown text drawn from engine state');
}
// --- 8255 backend ---
{
  const ports = new T.SimulatedPorts(), be = new T.I8255Backend(ports);
  ok(be.initialize() && be.isAvailable() && ports.regs[0x86] === 0x80, 'control word 80H');
  be.applyState({ state: S.GREEN, direction: D.LEFT }); ok(ports.regs[0x80] === 1 && ports.regs[0x82] === 1, 'GREEN -> PA0');
  be.applyState({ state: S.YELLOW, direction: D.LEFT }); ok(ports.regs[0x80] === 2, 'YELLOW -> PA1');
  be.applyState({ state: S.RED, direction: D.ALL }); ok(ports.regs[0x80] === 4 && ports.regs[0x82] === 7, 'RED -> PA2, ALL -> PB=7');
  be.allOff(); ok(ports.regs[0x80] === 0 && ports.regs[0x82] === 0, 'all off');
  const p2 = new T.SimulatedPorts(), b2 = new T.I8255Backend(p2), e = fresh(); b2.initialize(); e.setHardware(b2);
  ok(p2.regs[0x80] === 1, 'engine drives chip'); e.update(10); ok(p2.regs[0x80] === 2, 'yellow'); e.update(3); ok(p2.regs[0x80] === 4, 'red');
}
// --- fallback + serial ---
{
  const sim = new T.SimulatedHardware();
  let sel = T.selectHardware(new T.I8255Backend(new T.SimulatedPorts(false)), sim);
  ok(sel.hw === sim && sel.note.includes('unavailable'), 'missing 8255 -> simulation');
  sel = T.selectHardware(new T.I8255Backend(new T.SimulatedPorts(true, false)), sim); ok(sel.hw === sim, 'closed port -> simulation');
  const good = new T.I8255Backend(new T.SimulatedPorts()); sel = T.selectHardware(good, sim); ok(sel.hw === good && sel.note === '', 'good 8255 chosen');
  sel = T.selectHardware(null, sim); ok(sel.hw === sim, 'no preferred backend');
  const sink = new T.LogSink(), sb = new T.SerialBackend(sink); sb.initialize();
  ok(T.SerialBackend.encode({ state: S.RED, direction: D.STRAIGHT_RIGHT }) === 'T R 6\n', 'serial encoding');
  sb.applyState({ state: S.RED, direction: 6 }); const n = sink.data.length; sb.applyState({ state: S.RED, direction: 6 }); ok(sink.data.length === n, 'serial dedupes');
  ok(T.SerialBackend.encode({ state: S.OFF, direction: 7 }) === 'T O 0\n', 'serial off');
}
// --- controller (command layer) without a browser ---
{
  const canvas = { getContext: () => new Proxy({}, { get: () => () => ({ addColorStop() {} }), set: () => true }), clientWidth: 800, clientHeight: 600, width: 800, height: 600 };
  global.window = { devicePixelRatio: 1 };
  const c = new SignalController(canvas);
  c.dispatch('red'); ok(c.status().state === 'RED', 'cmd red');
  c.dispatch('left'); ok(c.status().direction === 'LEFT', 'cmd left');
  c.dispatch('zoomIn'); near(c.status().zoom, 1.25, 'cmd zoomIn'); c.dispatch('zoomReset'); near(c.status().zoom, 1, 'cmd zoomReset');
  c.dispatch('pause'); ok(c.status().paused, 'cmd pause'); c.dispatch('mode'); ok(c.status().mode === 'MANUAL', 'cmd mode');
  c.dispatch('bogus'); ok(c.status().state === 'RED', 'unknown command ignored');
  ok(c.setTimings({ green: 20, yellow: 4, red: 12 }) === '' && c.status().timings.green === 20, 'setTimings');
  ok(c.setTimings({ green: 0, yellow: 4, red: 12 }) !== '' && c.status().timings.green === 20, 'bad timings change nothing');
  c.setZoom(99); near(c.status().zoom, 3, 'zoom clamped');
  c.setBackend('8255'); ok(c.status().backend === '8255' && c.hardwareView().red, '8255 backend active, shows RED');
  c.setBackend('8255-missing'); ok(c.status().backend === 'simulated' && /unavailable/.test(c.status().message), 'missing chip -> fallback message');
  c.dispatch('exit'); ok(c.status().stopped && c.status().state === 'OFF', 'exit stops and turns lamps off'); c.dispatch('green'); ok(c.status().state === 'OFF', 'commands ignored while stopped');
  c.dispatch('start'); ok(!c.status().stopped && c.status().state === 'GREEN', 'start resumes');
}
console.log(`${run - failed}/${run} checks passed`);
process.exit(failed ? 1 : 0);
