/* hardware.js - hardware backends (JavaScript twins of hardware/*.hpp).
 *
 *   SimulatedHardware  virtual LEDs                       (always available)
 *   I8255Backend       8255 PPI driver over a PortIO      (ports 80H..86H live ONLY here)
 *   SimulatedPorts     in-memory 8255 chip with a write log (or "missing chip" for fallback demos)
 *   SerialBackend      "T <G|Y|R|O> <mask>\n" protocol for a microcontroller (sink is pluggable)
 *   selectHardware     use the preferred backend, else fall back to simulation
 */
(function (root) {
  'use strict';
  const T = (typeof module !== 'undefined' && module.exports) ? require('./core.js') : root.Traffic;
  const { LightState } = T;

  class SimulatedHardware {
    constructor() { this.ready = false; this.red = false; this.yellow = false; this.green = false; this.directionMask = 0; this.applyCount = 0; }
    get name() { return 'simulated'; }
    initialize() { this.ready = true; return true; }
    isAvailable() { return this.ready; }
    applyState(s) {
      this.applyCount++;
      if (s.state === LightState.OFF) { this._clear(); return; }
      this.green = s.state === LightState.GREEN;
      this.yellow = s.state === LightState.YELLOW;
      this.red = s.state === LightState.RED;
      this.directionMask = s.direction;
    }
    allOff() { this._clear(); this.applyCount++; }
    shutdown() { this.allOff(); }
    _clear() { this.red = this.yellow = this.green = false; this.directionMask = 0; }
  }

  const I8255 = Object.freeze({
    PORT_A: 0x80, PORT_B: 0x82, PORT_C: 0x84, CONTROL: 0x86,
    CONTROL_WORD_ALL_OUTPUT_MODE0: 0x80,
    BIT_GREEN: 0x01, BIT_YELLOW: 0x02, BIT_RED: 0x04,
  });

  /** In-memory 8255. present=false emulates a missing chip (reads float to 0xFF). */
  class SimulatedPorts {
    constructor(present = true, open = true) {
      this.present = present; this.open = open;
      this.regs = { 0x80: 0, 0x82: 0, 0x84: 0, 0x86: 0 };
      this.log = [];                       // [{port, value}] most recent last
      this.onWrite = null;
    }
    isOpen() { return this.open; }
    write(port, value) {
      this.log.push({ port, value: value & 0xFF });
      if (this.log.length > 500) this.log.shift();
      if (this.present && port in this.regs) this.regs[port] = value & 0xFF;
      if (this.onWrite) this.onWrite(port, value & 0xFF);
    }
    read(port) { return this.present && port in this.regs ? this.regs[port] : 0xFF; }
  }

  class I8255Backend {
    constructor(io) { this.io = io; this.available = false; }
    get name() { return '8255'; }
    static portAValue(state) {
      return state === LightState.GREEN ? I8255.BIT_GREEN
        : state === LightState.YELLOW ? I8255.BIT_YELLOW
        : state === LightState.RED ? I8255.BIT_RED : 0;
    }
    static portBValue(s) { return s.state === LightState.OFF ? 0 : s.direction; }
    initialize() {
      this.available = false;
      if (!this.io.isOpen()) return false;
      this.io.write(I8255.CONTROL, I8255.CONTROL_WORD_ALL_OUTPUT_MODE0);
      this.io.write(I8255.PORT_A, 0xA5);                       // read-back probe
      this.available = this.io.read(I8255.PORT_A) === 0xA5;
      this.io.write(I8255.PORT_A, 0);
      this.io.write(I8255.PORT_B, 0);
      return this.available;
    }
    isAvailable() { return this.available; }
    applyState(s) {
      if (!this.available) return;
      this.io.write(I8255.PORT_A, I8255Backend.portAValue(s.state));
      this.io.write(I8255.PORT_B, I8255Backend.portBValue(s));
    }
    allOff() {
      if (!this.available) return;
      this.io.write(I8255.PORT_A, 0);
      this.io.write(I8255.PORT_B, 0);
    }
    shutdown() { this.allOff(); }
  }

  class LogSink {
    constructor() { this.data = ''; this.open = true; }
    isOpen() { return this.open; }
    write(bytes) { this.data += bytes; return true; }
  }

  class SerialBackend {
    constructor(sink) { this.sink = sink; this.available = false; this.last = ''; }
    get name() { return 'serial'; }
    static encode(s) {
      const c = s.state === LightState.GREEN ? 'G' : s.state === LightState.YELLOW ? 'Y'
        : s.state === LightState.RED ? 'R' : 'O';
      return `T ${c} ${s.state === LightState.OFF ? 0 : s.direction}\n`;
    }
    initialize() { this.available = this.sink.isOpen(); if (this.available) this.allOff(); return this.available; }
    isAvailable() { return this.available; }
    applyState(s) {
      if (!this.available) return;
      const line = SerialBackend.encode(s);
      if (line !== this.last) { this.last = line; this.sink.write(line); }
    }
    allOff() { if (this.available) { this.last = 'T O 0\n'; this.sink.write(this.last); } }
    shutdown() { this.allOff(); }
  }

  /** Use `preferred` if it initialises and is available; otherwise initialise `fallback`. */
  function selectHardware(preferred, fallback) {
    if (preferred && preferred.initialize() && preferred.isAvailable()) return { hw: preferred, note: '' };
    fallback.initialize();
    return { hw: fallback, note: `${preferred ? preferred.name : '(none)'} unavailable; using ${fallback.name}` };
  }

  const api = { SimulatedHardware, SimulatedPorts, I8255Backend, I8255, SerialBackend, LogSink, selectHardware };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Traffic = Object.assign(root.Traffic || {}, api);
})(typeof globalThis !== 'undefined' ? globalThis : this);
