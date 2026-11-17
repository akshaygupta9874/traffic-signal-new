/* controller.js - UI-agnostic glue shared by the desktop and mobile frontends.
 *
 * Owns: engine + hardware backend + view transform + render loop.
 * Pages only (1) create a controller for a <canvas>, (2) bind buttons with
 * data-cmd="..." and (3) show controller.status() when onChange fires.
 *
 * Commands: green yellow red off | left straight right none all cycle |
 *           zoomIn zoomOut zoomReset | pause mode reset exit start
 */
(function (root) {
  'use strict';
  const T = (typeof module !== 'undefined' && module.exports) ? require('./core.js') : root.Traffic;
  if (typeof module !== 'undefined' && module.exports) { Object.assign(T, require('./hardware.js'), require('./renderer.js')); }
  const { LightState, Direction, DIRECTION_NAMES, LIGHT_NAMES, LIMITS } = T;

  const KEYMAP = {
    g: 'green', y: 'yellow', r: 'red', l: 'left', s: 'straight', d: 'right', n: 'none', a: 'all', c: 'cycle',
    '+': 'zoomIn', '=': 'zoomIn', '-': 'zoomOut', '_': 'zoomOut', '0': 'zoomReset',
    ' ': 'pause', m: 'mode', Escape: 'exit', Enter: 'start',
  };

  class SignalController {
    constructor(canvas, opts = {}) {
      this.canvas = canvas;
      this.ctx = canvas.getContext('2d');
      this.renderer = new T.CanvasRenderer(this.ctx);
      this.transform = new T.ViewTransform(1);
      this.view = new T.TrafficLightView(this.renderer, this.transform);
      this.onChange = opts.onChange || (() => {});
      this.stopped = false;
      this.message = '';
      this._lastKey = '';
      this._raf = 0;
      this._last = 0;
      this._sim = new T.SimulatedHardware();
      this.loadConfig(opts.config || T.defaultConfig());
    }

    /** (Re)creates the engine from a config object. Invalid config -> defaults + message. */
    loadConfig(cfg) {
      this.engine = new T.TrafficEngine(cfg);
      const r = this.engine.initialize();
      this.message = r.ok ? '' : `Invalid configuration (${r.error}); defaults used.`;
      this.transform.setScale(this.engine.config.zoom) || this.transform.resetZoom();
      this.stopped = false;
      this.setBackend(this.backendName || 'simulated');
    }

    /** name: 'simulated' | '8255' | '8255-missing' | 'serial'. Falls back to simulation when unavailable. */
    setBackend(name) {
      if (this.hw) this.hw.shutdown();
      this.backendName = name;
      this.ports = new T.SimulatedPorts(name !== '8255-missing');
      this.serialSink = new T.LogSink();
      let preferred = null;
      if (name === '8255' || name === '8255-missing') preferred = new T.I8255Backend(this.ports);
      else if (name === 'serial') preferred = new T.SerialBackend(this.serialSink);
      if (preferred) {
        const sel = T.selectHardware(preferred, this._sim);
        this.hw = sel.hw; this.message = sel.note || '';
      } else { this._sim.initialize(); this.hw = this._sim; this.message = ''; }
      this.engine.setHardware(this.hw);
      this._changed(true);
    }

    dispatch(cmd) {
      const e = this.engine, t = this.transform;
      if (this.stopped && cmd !== 'start' && cmd !== 'reset') return;
      switch (cmd) {
        case 'green': e.setState(LightState.GREEN); break;
        case 'yellow': e.setState(LightState.YELLOW); break;
        case 'red': e.setState(LightState.RED); break;
        case 'off': e.setState(LightState.OFF); break;
        case 'left': e.setDirection(Direction.LEFT); break;
        case 'straight': e.setDirection(Direction.STRAIGHT); break;
        case 'right': e.setDirection(Direction.RIGHT); break;
        case 'none': e.setDirection(Direction.NONE); break;
        case 'all': e.setDirection(Direction.ALL); break;
        case 'cycle': e.setDirection(T.nextDirection(e.getDirection())); break;
        case 'zoomIn': t.zoomIn(); break;
        case 'zoomOut': t.zoomOut(); break;
        case 'zoomReset': t.resetZoom(); break;
        case 'pause': e.setPaused(!e.isPaused()); break;
        case 'mode': e.setMode(e.mode === 'AUTO' ? 'MANUAL' : 'AUTO'); break;
        case 'reset': this.stopped = false; e.reset(); break;
        case 'exit': this.stopped = true; e.allOff(); this.hw.shutdown(); break;   // "ESC": everything dark
        case 'start': if (this.stopped) { this.stopped = false; e.reset(); } break;
        default: return;
      }
      this._changed(true);
    }

    /** Validates all three before applying any. Returns '' or an error message. */
    setTimings({ green, yellow, red }) {
      const probe = Object.assign(T.defaultConfig(), { greenDuration: green, yellowDuration: yellow, redDuration: red });
      const why = T.validateConfig(probe);
      if (why) return why;
      this.engine.setDuration(LightState.GREEN, green);
      this.engine.setDuration(LightState.YELLOW, yellow);
      this.engine.setDuration(LightState.RED, red);
      this._changed(true);
      return '';
    }

    setZoom(value) {
      const v = Math.min(LIMITS.maxZoom, Math.max(LIMITS.minZoom, value));
      this.transform.setScale(v); this._changed(true);
    }

    /** What the connected hardware currently shows (for the LED / port monitor). */
    hardwareView() {
      if (this.hw.name === '8255') {
        const a = this.ports.regs[0x80], b = this.ports.regs[0x82];
        return { green: !!(a & 1), yellow: !!(a & 2), red: !!(a & 4), mask: b & 7,
                 regs: { A: a, B: b, C: this.ports.regs[0x84], CTRL: this.ports.regs[0x86] }, log: this.ports.log.slice(-8) };
      }
      if (this.hw.name === 'serial') {
        const m = /T (\w) (\d)/.exec(this.hw.last || '') || [];
        return { green: m[1] === 'G', yellow: m[1] === 'Y', red: m[1] === 'R', mask: Number(m[2] || 0), regs: null,
                 log: this.serialSink.data.trim().split('\n').slice(-8).map((l) => ({ text: l })) };
      }
      const s = this._sim;
      return { green: s.green, yellow: s.yellow, red: s.red, mask: s.directionMask, regs: null, log: [] };
    }

    status() {
      const s = this.engine.snapshot();
      return {
        state: LIGHT_NAMES[s.state], stateCode: s.state, direction: DIRECTION_NAMES[s.direction], directionCode: s.direction,
        remaining: Math.max(0, Math.ceil(s.remainingTime - 1e-9)), duration: s.duration,
        zoom: this.transform.scale, mode: this.engine.mode, paused: this.engine.isPaused(), stopped: this.stopped,
        backend: this.hw.name, requestedBackend: this.backendName, message: this.message,
        timings: { green: this.engine.config.greenDuration, yellow: this.engine.config.yellowDuration, red: this.engine.config.redDuration },
      };
    }

    // ---- render loop -----------------------------------------------------
    start() {
      if (this._raf) return;
      this._last = performance.now();
      const frame = (now) => {
        const dt = (now - this._last) / 1000;
        this._last = now;
        if (!this.stopped) this.engine.update(dt);
        this.draw();
        this._changed(false);
        this._raf = requestAnimationFrame(frame);
      };
      this._raf = requestAnimationFrame(frame);
    }
    stop() { if (this._raf) cancelAnimationFrame(this._raf); this._raf = 0; }

    draw() {
      const c = this.canvas, dpr = window.devicePixelRatio || 1;
      const w = c.clientWidth, h = c.clientHeight;
      if (c.width !== Math.round(w * dpr) || c.height !== Math.round(h * dpr)) { c.width = Math.round(w * dpr); c.height = Math.round(h * dpr); }
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      this.view.renderFrame(this.engine.snapshot(), w, h);
      if (this.stopped) {
        this.renderer.drawText(w / 2 - 70, h / 2 - 12, 'STOPPED', 24, T.COLORS.muted, '700');
      }
    }

    _changed(force) {
      const s = this.status();
      const key = [s.stateCode, s.directionCode, s.remaining, s.zoom.toFixed(3), s.mode, s.paused, s.stopped, s.backend, s.message].join('|');
      if (force || key !== this._lastKey) { this._lastKey = key; this.onChange(s, this); }
    }

    // ---- input -----------------------------------------------------------
    attachKeyboard(target) {
      target.addEventListener('keydown', (ev) => {
        const tag = (ev.target && ev.target.tagName) || '';
        if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT' || ev.ctrlKey || ev.metaKey || ev.altKey) return;
        const cmd = KEYMAP[ev.key.length === 1 ? ev.key.toLowerCase() : ev.key];
        if (cmd) { ev.preventDefault(); this.dispatch(cmd); }
      });
    }

    /** Mouse wheel and two-finger pinch zoom on `el`. */
    attachGestures(el) {
      el.style.touchAction = 'none';
      el.addEventListener('wheel', (ev) => {
        ev.preventDefault();
        const dy = ev.deltaMode === 1 ? ev.deltaY * 16 : ev.deltaY;      // lines -> pixels
        this.setZoom(this.transform.scale * Math.exp(-dy * 0.0015));      // proportional: smooth on trackpads
      }, { passive: false });
      const pointers = new Map();
      let startDist = 0, startScale = 1;
      const dist = () => { const [a, b] = [...pointers.values()]; return Math.hypot(a.x - b.x, a.y - b.y); };
      el.addEventListener('pointerdown', (ev) => {
        pointers.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
        if (pointers.size === 2) { startDist = dist(); startScale = this.transform.scale; }
      });
      el.addEventListener('pointermove', (ev) => {
        if (!pointers.has(ev.pointerId)) return;
        pointers.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
        if (pointers.size === 2 && startDist > 0) this.setZoom(startScale * dist() / startDist);
      });
      const up = (ev) => { pointers.delete(ev.pointerId); startDist = 0; };
      el.addEventListener('pointerup', up);
      el.addEventListener('pointercancel', up);
    }

    /** Wires every [data-cmd] element under `rootEl` to dispatch(). */
    bindCommands(rootEl) {
      rootEl.querySelectorAll('[data-cmd]').forEach((b) => b.addEventListener('click', () => this.dispatch(b.dataset.cmd)));
    }
  }

  const api = { SignalController, KEYMAP };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Traffic = Object.assign(root.Traffic || {}, api);
})(typeof globalThis !== 'undefined' ? globalThis : this);
