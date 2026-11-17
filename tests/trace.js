// JavaScript twin of examples/trace.cpp (same script, same output format).
const T = require('../platform/desktop/js/core.js');
const [g, y, r, dt] = process.argv.slice(2).map(Number);
const cfg = T.defaultConfig();
if (process.argv.length >= 6) { cfg.greenDuration = g; cfg.yellowDuration = y; cfg.redDuration = r; }
const e = new T.TrafficEngine(cfg);
e.initialize();
const step = process.argv.length >= 6 ? dt : 0.25;
const out = [];
for (let i = 0; i <= 400; i++) {
  out.push(`${i} ${T.LIGHT_NAMES[e.getState()]} ${T.DIRECTION_NAMES[e.getDirection()]} ${Math.max(0, Math.ceil(e.getRemainingTime() - 1e-9))}`);
  if (i === 100) e.setState(T.LightState.RED);
  if (i === 150) e.setDirection(T.Direction.LEFT_RIGHT);
  if (i === 200) e.setPaused(true);
  if (i === 210) e.setPaused(false);
  if (i === 300) e.allOff();
  if (i === 320) e.reset();
  e.update(step);
}
console.log(out.join('\n'));
