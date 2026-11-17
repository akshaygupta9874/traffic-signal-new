/* core.js - platform-independent traffic engine (JavaScript twin of core/traffic_engine.cpp).
 *
 * Knows: state, direction, durations, remaining time.
 * Does NOT know: canvas, DOM, ports, serial, DOS. Outputs go through a hardware
 * object that implements { name, initialize(), isAvailable(), applyState(s), allOff(), shutdown() }.
 *
 * Works in a browser (global `Traffic`) and in Node (`require('./core.js')`).
 */
(function (root) {
  'use strict';

  const LightState = Object.freeze({ GREEN: 0, YELLOW: 1, RED: 2, OFF: 3 });
  const LIGHT_NAMES = ['GREEN', 'YELLOW', 'RED', 'OFF'];

  // 3-bit mask: LEFT=1, STRAIGHT=2, RIGHT=4 (same values as C++ and the 8086 code).
  const Direction = Object.freeze({
    NONE: 0, LEFT: 1, STRAIGHT: 2, STRAIGHT_LEFT: 3, RIGHT: 4,
    LEFT_RIGHT: 5, STRAIGHT_RIGHT: 6, ALL: 7,
  });
  const DIRECTION_NAMES = ['NONE', 'LEFT', 'STRAIGHT', 'STRAIGHT_LEFT', 'RIGHT',
                           'LEFT_RIGHT', 'STRAIGHT_RIGHT', 'ALL'];

  const LIMITS = Object.freeze({ minDuration: 1, maxDuration: 3600, minZoom: 0.5, maxZoom: 3.0 });
  const EPS = 1e-9;

  const isValidState = (s) => Number.isInteger(s) && s >= 0 && s <= 3;
  const isValidDirection = (d) => Number.isInteger(d) && d >= 0 && d <= 7;
  const hasLeft = (d) => (d & 1) !== 0;
  const hasStraight = (d) => (d & 2) !== 0;
  const hasRight = (d) => (d & 4) !== 0;
  const nextDirection = (d) => (d + 1) & 7;

  function parseDirection(text) {
    const i = DIRECTION_NAMES.indexOf(String(text).trim().toUpperCase());
    return i < 0 ? null : i;
  }

  function defaultConfig() {
    return { greenDuration: 10, yellowDuration: 3, redDuration: 10, zoom: 1.0,
             direction: Direction.STRAIGHT, mode: 'AUTO' };
  }

  const durationOk = (d) => typeof d === 'number' && Number.isFinite(d) &&
                            d >= LIMITS.minDuration && d <= LIMITS.maxDuration;

  /** Returns '' when valid, otherwise a human-readable reason. */
  function validateConfig(c) {
    const err = [];
    if (!durationOk(c.greenDuration)) err.push(`green duration must be ${LIMITS.minDuration}..${LIMITS.maxDuration} s`);
    if (!durationOk(c.yellowDuration)) err.push(`yellow duration must be ${LIMITS.minDuration}..${LIMITS.maxDuration} s`);
    if (!durationOk(c.redDuration)) err.push(`red duration must be ${LIMITS.minDuration}..${LIMITS.maxDuration} s`);
    if (!(Number.isFinite(c.zoom) && c.zoom >= LIMITS.minZoom && c.zoom <= LIMITS.maxZoom))
      err.push(`zoom must be ${LIMITS.minZoom}..${LIMITS.maxZoom}`);
    if (!isValidDirection(c.direction)) err.push('invalid direction');
    if (c.mode !== 'AUTO' && c.mode !== 'MANUAL') err.push('mode must be AUTO or MANUAL');
    return err.join('; ');
  }

  /** Same "key = value" format as examples/default.cfg. Returns {ok, config, error}. */
  function parseConfig(text, base) {
    const cfg = Object.assign(defaultConfig(), base || {});
    const lines = String(text).split(/\r?\n/);
    for (let n = 0; n < lines.length; n++) {
      const line = lines[n].replace(/#.*/, '').trim();
      if (!line) continue;
      const eq = line.indexOf('=');
      if (eq < 0) return { ok: false, error: `line ${n + 1}: expected key = value` };
      const key = line.slice(0, eq).trim();
      const val = line.slice(eq + 1).trim();
      if (['green', 'yellow', 'red', 'zoom'].includes(key)) {
        const num = Number(val);
        if (val === '' || !Number.isFinite(num)) return { ok: false, error: `line ${n + 1}: '${val}' is not a number` };
        if (key === 'green') cfg.greenDuration = num;
        else if (key === 'yellow') cfg.yellowDuration = num;
        else if (key === 'red') cfg.redDuration = num;
        else cfg.zoom = num;
      } else if (key === 'direction') {
        const d = parseDirection(val);
        if (d === null) return { ok: false, error: `line ${n + 1}: unknown direction '${val}'` };
        cfg.direction = d;
      } else if (key === 'mode') {
        const m = val.toUpperCase();
        if (m !== 'AUTO' && m !== 'MANUAL') return { ok: false, error: `line ${n + 1}: mode must be auto or manual` };
        cfg.mode = m;
      } else {
        return { ok: false, error: `line ${n + 1}: unknown key '${key}'` };
      }
    }
    const why = validateConfig(cfg);
    return why ? { ok: false, error: why } : { ok: true, config: cfg };
  }

  class TrafficEngine {
    constructor(config) {
      this._config = Object.assign(defaultConfig(), config || {});
      this._hw = null;
      this._paused = false;
      this._mode = this._config.mode;
      this._state = { state: LightState.GREEN, direction: this._config.direction, remainingTime: 0, duration: 0 };
      this._enter(LightState.GREEN);
    }

    /** Validates config; invalid -> defaults + returns the reason. Returns {ok, error}. */
    initialize() {
      const why = validateConfig(this._config);
      if (why) this._config = defaultConfig();
      this._mode = this._config.mode;
      this._paused = false;
      this._state.direction = this._config.direction;
      this._enter(LightState.GREEN);
      this._publish();
      return why ? { ok: false, error: why } : { ok: true, error: '' };
    }

    setHardware(hw) { this._hw = hw || null; this._publish(); }

    /** Advance time (seconds). Ignores negative / NaN. Handles deltas spanning phases. */
    update(dt) {
      if (typeof dt !== 'number' || !Number.isFinite(dt) || dt <= 0) return;
      if (this._paused || this._mode !== 'AUTO' || this._state.state === LightState.OFF) return;
      const c = this._config;
      const cycle = c.greenDuration + c.yellowDuration + c.redDuration;
      if (dt > cycle) dt -= Math.floor(dt / cycle) * cycle;
      while (dt > EPS) {
        if (dt < this._state.remainingTime - EPS) { this._state.remainingTime -= dt; break; }
        dt -= this._state.remainingTime;
        this._enter(TrafficEngine._next(this._state.state));
      }
      this._publish();
    }

    setState(s) {
      if (!isValidState(s)) return false;
      if (s === LightState.OFF) { this.allOff(); return true; }
      this._enter(s);
      this._publish();
      return true;
    }
    setDirection(d) {
      if (!isValidDirection(d)) return false;
      this._state.direction = d;
      this._publish();
      return true;
    }
    setDuration(s, seconds) {
      if (!durationOk(seconds)) return false;
      if (s === LightState.GREEN) this._config.greenDuration = seconds;
      else if (s === LightState.YELLOW) this._config.yellowDuration = seconds;
      else if (s === LightState.RED) this._config.redDuration = seconds;
      else return false;
      return true;
    }
    allOff() {
      this._state.state = LightState.OFF;
      this._state.duration = 0;
      this._state.remainingTime = 0;
      if (this._hw) this._hw.allOff();
    }
    reset() {
      this._paused = false;
      this._mode = this._config.mode;
      this._state.direction = this._config.direction;
      this._enter(LightState.GREEN);
      this._publish();
    }

    setPaused(p) { this._paused = !!p; }
    isPaused() { return this._paused; }
    setMode(m) { if (m === 'AUTO' || m === 'MANUAL') this._mode = m; }
    get mode() { return this._mode; }

    getState() { return this._state.state; }
    getDirection() { return this._state.direction; }
    getRemainingTime() { return this._state.remainingTime; }
    /** Read-only copy: renderers/hardware can never mutate the engine through it. */
    snapshot() { return Object.freeze(Object.assign({}, this._state)); }
    get config() { return Object.assign({}, this._config); }

    _enter(s) {
      this._state.state = s;
      this._state.duration = s === LightState.GREEN ? this._config.greenDuration
        : s === LightState.YELLOW ? this._config.yellowDuration
        : s === LightState.RED ? this._config.redDuration : 0;
      this._state.remainingTime = this._state.duration;
    }
    _publish() { if (this._hw) this._hw.applyState(this.snapshot()); }
    static _next(s) {
      return s === LightState.GREEN ? LightState.YELLOW : s === LightState.YELLOW ? LightState.RED : LightState.GREEN;
    }
  }

  const api = {
    LightState, LIGHT_NAMES, Direction, DIRECTION_NAMES, LIMITS,
    isValidState, isValidDirection, hasLeft, hasStraight, hasRight, nextDirection, parseDirection,
    defaultConfig, validateConfig, parseConfig, TrafficEngine,
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Traffic = Object.assign(root.Traffic || {}, api);
})(typeof globalThis !== 'undefined' ? globalThis : this);
