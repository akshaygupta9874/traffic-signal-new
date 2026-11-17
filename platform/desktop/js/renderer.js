/* renderer.js - drawing layer (JavaScript twin of rendering/*.hpp).
 *
 *   ViewTransform     logical -> screen mapping + zoom (screen = origin + logical * scale)
 *   CanvasRenderer    drawing primitives on a 2D canvas context (screen pixels)
 *   TrafficLightView  turns an engine snapshot into primitives; never computes timing
 *
 * Any object with the same primitive methods can replace CanvasRenderer
 * (SVG, WebGL, a native canvas on Android...).
 */
(function (root) {
  'use strict';
  const T = (typeof module !== 'undefined' && module.exports) ? require('./core.js') : root.Traffic;
  const { LightState, LIMITS, hasLeft, hasStraight, hasRight } = T;

  class ViewTransform {
    constructor(scale = 1.0) { this.zoomStep = 0.25; this._scale = 1.0; if (!this.setScale(scale)) this._scale = 1.0; this.originX = 0; this.originY = 0; }
    get scale() { return this._scale; }
    setScale(s) {
      if (typeof s !== 'number' || !Number.isFinite(s) || s < LIMITS.minZoom || s > LIMITS.maxZoom) return false;
      this._scale = s; return true;
    }
    _clamp(v) { return Math.min(LIMITS.maxZoom, Math.max(LIMITS.minZoom, v)); }
    zoomIn() { this._scale = this._clamp(this._scale + this.zoomStep); }
    zoomOut() { this._scale = this._clamp(this._scale - this.zoomStep); }
    resetZoom() { this._scale = 1.0; }
    setOrigin(x, y) { this.originX = x; this.originY = y; }
    sx(lx) { return this.originX + lx * this._scale; }
    sy(ly) { return this.originY + ly * this._scale; }
    len(l) { return l * this._scale; }
  }

  const rgb = (c) => `rgb(${c[0]},${c[1]},${c[2]})`;

  class CanvasRenderer {
    constructor(ctx) { this.ctx = ctx; }
    clear(color, w, h) { this.ctx.fillStyle = rgb(color); this.ctx.fillRect(0, 0, w, h); }
    drawCircle(cx, cy, r, color, filled = true, lw = 1) {
      const c = this.ctx; c.beginPath(); c.arc(cx, cy, Math.max(0, r), 0, Math.PI * 2);
      if (filled) { c.fillStyle = rgb(color); c.fill(); } else { c.strokeStyle = rgb(color); c.lineWidth = lw; c.stroke(); }
    }
    drawRectangle(x, y, w, h, color, filled = true, lw = 1) {
      const c = this.ctx, rad = Math.min(w, h) * 0.08;
      c.beginPath();
      if (c.roundRect) c.roundRect(x, y, w, h, rad); else c.rect(x, y, w, h);
      if (filled) { c.fillStyle = rgb(color); c.fill(); } else { c.strokeStyle = rgb(color); c.lineWidth = lw; c.stroke(); }
    }
    drawLine(x1, y1, x2, y2, color, lw = 1) {
      const c = this.ctx; c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2);
      c.strokeStyle = rgb(color); c.lineWidth = lw; c.stroke();
    }
    drawPolygon(pts, color) {
      const c = this.ctx; c.beginPath(); pts.forEach((p, i) => (i ? c.lineTo(p.x, p.y) : c.moveTo(p.x, p.y)));
      c.closePath(); c.fillStyle = rgb(color); c.fill();
    }
    drawText(x, y, text, size, color, weight = '600') {
      const c = this.ctx; c.font = `${weight} ${size}px ui-monospace, Menlo, Consolas, monospace`;
      c.textBaseline = 'top'; c.fillStyle = rgb(color); c.fillText(text, x, y);
    }
    /** Optional soft light around a lit lamp. */
    drawGlow(cx, cy, r, color) {
      const c = this.ctx, g = c.createRadialGradient(cx, cy, r * 0.6, cx, cy, r * 2.4);
      g.addColorStop(0, `rgba(${color[0]},${color[1]},${color[2]},0.55)`);
      g.addColorStop(1, `rgba(${color[0]},${color[1]},${color[2]},0)`);
      c.fillStyle = g; c.beginPath(); c.arc(cx, cy, r * 2.4, 0, Math.PI * 2); c.fill();
    }
    /** Filled arrow centred on (cx, cy). heading: 'up' | 'left' | 'right'. Built from a polygon. */
    drawArrow(cx, cy, length, thickness, heading, color) {
      const hl = length * 0.5, hw = length * 0.42, half = length / 2, t = thickness / 2;
      const uv = [[half, 0], [half - hl, hw], [half - hl, t], [-half, t], [-half, -t], [half - hl, -t], [half - hl, -hw]];
      const pts = uv.map(([u, v]) => heading === 'up' ? { x: cx + v, y: cy - u }
        : heading === 'left' ? { x: cx - u, y: cy + v } : { x: cx + u, y: cy + v });
      this.drawPolygon(pts, color);
    }
  }

  const COLORS = {
    background: [16, 20, 26], housing: [38, 40, 44], housingEdge: [106, 110, 118], pole: [85, 88, 94],
    text: [230, 232, 236], muted: [154, 160, 168],
    lamp: [[255, 45, 45], [255, 210, 31], [45, 224, 90]],    // red, yellow, green (top to bottom)
  };
  const dim = (c) => [c[0] >> 2, c[1] >> 2, c[2] >> 2];
  const LAYOUT = Object.freeze({
    housingHalfW: 28, housingHalfH: 56, lampRadius: 10, lampY: [-40, -14, 12],
    arrowY: 40, arrowDX: 16, poleHalfW: 4, poleTop: 56, poleBottom: 116,
  });

  function stateColor(s) {
    return s === LightState.RED ? COLORS.lamp[0] : s === LightState.YELLOW ? COLORS.lamp[1]
      : s === LightState.GREEN ? COLORS.lamp[2] : COLORS.muted;
  }

  class TrafficLightView {
    constructor(renderer, transform) { this.r = renderer; this.t = transform; }

    /** Draws the centred signal. The surrounding HUD is the page's job (or a text renderer's). */
    renderFrame(snapshot, w, h) {
      this.r.clear(COLORS.background, w, h);
      // The signal's vertical extent is -56..116 logical units; centre that block.
      this.t.setOrigin(w / 2, h / 2 - this.t.len(30));
      this.drawTrafficLight(snapshot);
    }
    drawTrafficLight(s) {
      this.drawPole(); this.drawHousing();
      for (let i = 0; i < 3; i++) {                       // i: 0 red, 1 yellow, 2 green
        this.drawLamp({ centerX: 0, centerY: LAYOUT.lampY[i], radius: LAYOUT.lampRadius,
                        color: COLORS.lamp[i], on: s.state === 2 - i });
      }
      this.drawDirectionArrow(s);
      this.drawCountdown(s);
    }
    drawHousing() {
      const L = LAYOUT, t = this.t;
      this.r.drawRectangle(t.sx(-L.housingHalfW), t.sy(-L.housingHalfH), t.len(2 * L.housingHalfW), t.len(2 * L.housingHalfH), COLORS.housing, true);
      this.r.drawRectangle(t.sx(-L.housingHalfW), t.sy(-L.housingHalfH), t.len(2 * L.housingHalfW), t.len(2 * L.housingHalfH), COLORS.housingEdge, false, Math.max(1, t.len(2)));
    }
    drawPole() {
      const L = LAYOUT, t = this.t;
      this.r.drawRectangle(t.sx(-L.poleHalfW), t.sy(L.poleTop), t.len(2 * L.poleHalfW), t.len(L.poleBottom - L.poleTop), COLORS.pole, true);
    }
    drawLamp(lamp) {
      const t = this.t, cx = t.sx(lamp.centerX), cy = t.sy(lamp.centerY), rad = t.len(lamp.radius);
      if (lamp.on && this.r.drawGlow) this.r.drawGlow(cx, cy, rad, lamp.color);
      this.r.drawCircle(cx, cy, rad + t.len(2), [0, 0, 0], true);                 // bezel
      this.r.drawCircle(cx, cy, rad, lamp.on ? lamp.color : dim(lamp.color), true);
      if (lamp.on) this.r.drawCircle(cx - rad * 0.3, cy - rad * 0.3, rad * 0.25, [255, 255, 255], true);
    }
    drawDirectionArrow(s) {
      if (s.direction === 0 || s.state === LightState.OFF) return;
      const L = LAYOUT, t = this.t, c = stateColor(s.state), len = t.len(15), th = t.len(4);
      if (hasLeft(s.direction)) this.r.drawArrow(t.sx(-L.arrowDX), t.sy(L.arrowY), len, th, 'left', c);
      if (hasStraight(s.direction)) this.r.drawArrow(t.sx(0), t.sy(L.arrowY), len, th, 'up', c);
      if (hasRight(s.direction)) this.r.drawArrow(t.sx(L.arrowDX), t.sy(L.arrowY), len, th, 'right', c);
    }
    /** Shows engine-computed remaining time; never counts anything itself. */
    drawCountdown(s) {
      if (s.state === LightState.OFF) return;
      const t = this.t, secs = Math.max(0, Math.ceil(s.remainingTime - 1e-9));
      this.r.drawText(t.sx(36), t.sy(-52), String(secs), t.len(30), stateColor(s.state), '700');
      this.r.drawText(t.sx(36), t.sy(-18), T.LIGHT_NAMES[s.state], t.len(10), COLORS.muted, '600');
    }
  }

  const api = { ViewTransform, CanvasRenderer, TrafficLightView, COLORS, LAYOUT };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Traffic = Object.assign(root.Traffic || {}, api);
})(typeof globalThis !== 'undefined' ? globalThis : this);
