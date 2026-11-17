/* signal.js - SVG drawing of the signal head and the direction arrows. Pure view: it is told
 * what to show ({state, mask}) and never computes time or decides anything. */
(function (root) {
  'use strict';
  const NS = 'http://www.w3.org/2000/svg';
  let uid = 0;
  function el(tag, attrs, parent) {
    const e = document.createElementNS(NS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    if (parent) parent.appendChild(e);
    return e;
  }
  const WORD = { G: 'Go', Y: 'Get ready', R: 'Stop', O: 'Signal off' };
  const NAME = { G: 'Green', Y: 'Yellow', R: 'Red', O: 'Off' };
  const LAMPS = {
    R: { cy: 80,  on: ['#FF9A90', '#FF4638', '#B8221A'], off: '#3A1511', glow: '#FF4638' },
    Y: { cy: 172, on: ['#FFE08A', '#FFB400', '#B77800'], off: '#392B0A', glow: '#FFB400' },
    G: { cy: 264, on: ['#9CF5C6', '#1FD97A', '#0F8F4D'], off: '#0F3022', glow: '#1FD97A' },
  };

  class SignalView {
    constructor(host) {
      const id = 'sv' + (uid++);
      const svg = el('svg', { viewBox: '0 0 200 420', role: 'img', 'aria-label': 'Traffic signal' }, host);
      svg.setAttribute('class', 'signal');
      const defs = el('defs', {}, svg);
      for (const k in LAMPS) {
        const L = LAMPS[k];
        const g = el('radialGradient', { id: `${id}-on-${k}`, cx: '38%', cy: '32%', r: '75%' }, defs);
        el('stop', { offset: '0', 'stop-color': L.on[0] }, g);
        el('stop', { offset: '.45', 'stop-color': L.on[1] }, g);
        el('stop', { offset: '1', 'stop-color': L.on[2] }, g);
        const gl = el('radialGradient', { id: `${id}-glow-${k}` }, defs);
        el('stop', { offset: '.45', 'stop-color': L.glow, 'stop-opacity': '.55' }, gl);
        el('stop', { offset: '1', 'stop-color': L.glow, 'stop-opacity': '0' }, gl);
      }
      el('rect', { x: 93, y: 330, width: 14, height: 90, class: 'pole' }, svg);
      el('rect', { x: 20, y: 6, width: 160, height: 334, rx: 36, class: 'housing' }, svg);
      this.lamps = {};
      for (const k of ['R', 'Y', 'G']) {
        const L = LAMPS[k];
        const glow = el('circle', { cx: 100, cy: L.cy, r: 84, fill: `url(#${id}-glow-${k})`, opacity: 0 }, svg);
        el('circle', { cx: 100, cy: L.cy, r: 49, class: 'bezel' }, svg);
        const lamp = el('circle', { cx: 100, cy: L.cy, r: 42, fill: L.off }, svg);
        const shine = el('ellipse', { cx: 86, cy: L.cy - 17, rx: 13, ry: 7, fill: '#fff', opacity: 0.06 }, svg);
        el('path', { d: `M ${100 - 52} ${L.cy - 12} A 54 54 0 0 1 ${100 + 52} ${L.cy - 12}`, class: 'visor' }, svg);
        this.lamps[k] = { lamp, glow, shine, id };
      }
      this.svg = svg;
    }
    /** st: 'G' | 'Y' | 'R' | 'O' */
    update(st) {
      for (const k in this.lamps) {
        const { lamp, glow, shine, id } = this.lamps[k];
        const on = st === k;
        lamp.setAttribute('fill', on ? `url(#${id}-on-${k})` : LAMPS[k].off);
        glow.setAttribute('opacity', on ? 1 : 0);
        shine.setAttribute('opacity', on ? 0.3 : 0.06);
      }
      this.svg.setAttribute('aria-label', 'Traffic signal: ' + (NAME[st] || 'unknown'));
    }
  }

  class DirectionView {
    constructor(host) {
      const svg = el('svg', { viewBox: '0 0 304 92', role: 'img', 'aria-label': 'Allowed directions' }, host);
      svg.setAttribute('class', 'dirview');
      this.tiles = [];
      const rot = [-90, 0, 90];                           // left, straight, right
      rot.forEach((r, i) => {
        const g = el('g', {}, svg);
        el('rect', { x: i * 104 + 2, y: 2, width: 92, height: 88, rx: 18, class: 'tile' }, g);
        const a = el('path', { d: 'M0,-31 L27,-3 H11 V29 H-11 V-3 H-27 Z', class: 'arrow',
                               transform: `translate(${i * 104 + 48},46) rotate(${r})` }, g);
        this.tiles.push(g);
      });
      this.svg = svg;
    }
    /** mask: bit0 left, bit1 straight, bit2 right. st = 'O' lights nothing. */
    update(mask, st) {
      const names = ['left', 'straight', 'right'], on = [];
      this.tiles.forEach((g, i) => {
        const lit = st !== 'O' && (mask >> (i === 0 ? 0 : i === 1 ? 1 : 2) & 1) === 1;
        g.setAttribute('class', lit ? 'on' : '');
        if (lit) on.push(names[i]);
      });
      this.svg.setAttribute('aria-label', 'Allowed directions: ' + (on.join(', ') || 'none'));
    }
  }

  root.SignalView = SignalView;
  root.DirectionView = DirectionView;
  root.TrafficText = { WORD, NAME };
})(window);
