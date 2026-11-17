/* link.js - WebSocket client shared by both apps.
 *
 * Events (register with .on(name, fn)):
 *   state(s)      a new S message from the MASM program (via the relay)
 *   status(o)     { live, reason }  live = socket open AND MASM link fresh
 *   presence(o)   { guards, controllers }
 *   latency(o)    { rtt, coreRtt }   browser<->relay and relay<->MASM, in ms
 *   applied(cmd, ms)   command confirmed by the MASM program; ms = click to confirmation
 *   rejected(cmd, code) / failed(cmd, reason)
 *   authfail()
 */
(function (root) {
  'use strict';
  const STALE_MS = 3500;

  class TrafficLink {
    constructor({ role = 'guard', pin = '' } = {}) {
      this.role = role; this.pin = pin;
      this.h = {};
      this.ws = null; this.wsUp = false; this.coreUp = false;
      this.state = null; this.rtt = null; this.coreRtt = null;
      this.guards = 0; this.controllers = 0;
      this.pending = new Map(); this.cid = 0;
      this.lastMsgAt = 0; this._retry = 250; this._stop = false; this._last = '';
    }
    on(n, fn) { this.h[n] = fn; return this; }
    _emit(n, ...a) { if (this.h[n]) this.h[n](...a); }
    get live() { return this.wsUp && this.coreUp; }

    connect() {
      this._stop = false;
      const custom = window.TrafficServer && TrafficServer.get();
      const proto = !custom && location.protocol === 'https:' ? 'wss' : 'ws';
      const host = window.TrafficServer ? TrafficServer.host() : location.host;
      const q = this.role === 'controller' ? `?role=controller&pin=${encodeURIComponent(this.pin)}` : '?role=guard';
      const ws = this.ws = new WebSocket(`${proto}://${host}/ws${q}`);
      ws.onopen = () => { this.wsUp = true; this._retry = 250; this.lastMsgAt = performance.now(); this._ping(); this._status(); };
      ws.onmessage = (e) => this._msg(JSON.parse(e.data));
      ws.onclose = () => {
        this.wsUp = false; this.coreUp = false; this._status();
        this.pending.forEach((p) => this._emit('failed', p.cmd, 'offline')); this.pending.clear();
        if (!this._stop) { setTimeout(() => this.connect(), this._retry); this._retry = Math.min(this._retry * 1.7, 2000); }
      };
      ws.onerror = () => {};
      if (!this._timers) {
        this._timers = [
          setInterval(() => this._ping(), 1000),
          setInterval(() => {                           // half-open socket (phone sleep, Wi-Fi drop)
            const now = performance.now();
            if (this.wsUp && now - this.lastMsgAt > STALE_MS && this.ws) this.ws.close();
            this.pending.forEach((p, id) => { if (now - p.t0 > 4000) { this.pending.delete(id); this._emit('failed', p.cmd, 'timeout'); } });
          }, 500),
        ];
        document.addEventListener('visibilitychange', () => {
          if (!document.hidden && !this.wsUp && !this._stop && (!this.ws || this.ws.readyState > 1)) this.connect();
        });
      }
    }
    _ping() { if (this.wsUp && this.ws.readyState === 1) this.ws.send(JSON.stringify({ t: 'ping', c: performance.now() })); }

    _status() {
      const reason = !this.wsUp ? 'offline' : !this.coreUp ? 'core' : '';
      const key = String(this.live) + reason;
      if (key === this._last) return;
      this._last = key;
      this._emit('status', { live: this.live, reason });
    }

    _msg(m) {
      this.lastMsgAt = performance.now();
      switch (m.t) {
        case 'hello': this.coreUp = !!m.link; this.guards = m.guards; this.controllers = m.controllers; this.coreRtt = m.coreRtt; this._status(); break;
        case 'L': this.coreUp = !!m.link; this._status(); break;
        case 'S': {
          this.coreUp = true; this.state = m; this.coreRtt = m.rtt;
          this._status();
          this._emit('state', m);
          break;
        }
        case 'ack': {                         // the relay: the MASM program has processed command <cid>
          const p = this.pending.get(m.cid);
          if (p) { this.pending.delete(m.cid); this._emit('applied', p.cmd, performance.now() - p.t0); }
          break;
        }
        case 'P': this.guards = m.guards; this.controllers = m.controllers; this._emit('presence', { guards: m.guards, controllers: m.controllers }); break;
        case 'pong': this.rtt = performance.now() - m.c; this.coreRtt = m.rtt; this._emit('latency', { rtt: this.rtt, coreRtt: this.coreRtt }); break;
        case 'E': {
          if (m.cid && this.pending.has(m.cid)) { const p = this.pending.get(m.cid); this.pending.delete(m.cid); this._emit('failed', p.cmd, m.code); }
          else if (/^\d$/.test(m.code)) {        // MASM refused the command; its ack follows, so drop pending now
            const first = this.pending.entries().next().value;
            if (first) { this.pending.delete(first[0]); this._emit('rejected', first[1].cmd, m.code); }
          }
          break;
        }
        case 'auth': if (!m.ok) { this._stop = true; this._emit('authfail'); } break;
      }
    }

    /** Send a command line (G Y R X Z A M P0 P1 D0..D7 "T g y r"). Returns the id, or null if offline. */
    send(cmd) {
      if (!this.wsUp || this.ws.readyState !== 1) { this._emit('failed', cmd, 'offline'); return null; }
      this.cid = (this.cid % 59999) + 1;
      this.pending.set(this.cid, { cmd, t0: performance.now() });
      this.ws.send(JSON.stringify({ t: 'cmd', cmd, cid: this.cid }));
      return this.cid;
    }
  }
  root.TrafficLink = TrafficLink;
})(window);
