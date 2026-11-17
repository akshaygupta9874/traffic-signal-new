/* server.js - which relay should this app talk to?
 *   - In a browser opened from the relay (http://PC:8080/guard/) it is simply that page's host.
 *   - In the Android app the pages are bundled inside the app, so the person types the PC's address once
 *     (e.g. 192.168.1.20:8080). It is remembered on the phone.
 */
(function (root) {
  'use strict';
  const KEY = 'trafficServer';
  const get = () => { try { return localStorage.getItem(KEY) || ''; } catch (e) { return ''; } };
  const set = (v) => { try { v ? localStorage.setItem(KEY, v) : localStorage.removeItem(KEY); } catch (e) {} };
  // Inside the Android app the page comes from Capacitor's built-in server (http://localhost, no port) or capacitor://.
  const native = !!root.Capacitor || /^(capacitor|file):$/.test(location.protocol) || (location.hostname === 'localhost' && location.port === '');

  /** "192.168.1.20", "http://192.168.1.20:8080/" -> "192.168.1.20:8080"; '' if it is not usable */
  function normalize(input) {
    let v = String(input || '').trim().replace(/^[a-z]+:\/\//i, '').replace(/\/.*$/, '');
    if (!/^[\w.-]+(:\d{1,5})?$/.test(v)) return '';
    if (!/:\d+$/.test(v)) v += ':8080';
    return v;
  }

  function ask(message) {
    return new Promise((resolve) => {
      const wrap = document.createElement('div');
      wrap.className = 'gate';
      wrap.innerHTML = `<form><h2>Server address</h2>
        <label class="dim" for="srvIn">${message || 'Enter the address of the PC that runs the traffic relay. The phone and the PC must be on the same Wi-Fi.'}</label>
        <input id="srvIn" type="text" inputmode="url" autocapitalize="off" autocomplete="off" spellcheck="false" placeholder="192.168.1.20:8080">
        <div class="err" role="alert"></div>
        <button class="btn primary" type="submit">Save and connect</button>
        ${get() ? '<button class="btn" type="button" data-x>Cancel</button>' : ''}</form>`;
      document.body.appendChild(wrap);
      const inp = wrap.querySelector('input'), err = wrap.querySelector('.err');
      inp.value = get(); inp.focus();
      wrap.querySelector('form').addEventListener('submit', (e) => {
        e.preventDefault();
        const v = normalize(inp.value);
        if (!v) { err.textContent = 'Use the format 192.168.1.20:8080'; return; }
        set(v); wrap.remove(); resolve(v);
      });
      const x = wrap.querySelector('[data-x]');
      if (x) x.addEventListener('click', () => { wrap.remove(); resolve(null); });
    });
  }

  root.TrafficServer = {
    get, set, native, normalize, ask,
    host: () => get() || location.host,
    needed: () => native && !get(),
    /** Ask for an address when the Android app has none, then continue. */
    async ensure() { if (this.needed()) await ask(); return this.host(); },
    /** Change it from a button. Reloads so the link reconnects cleanly. */
    async change() { const v = await ask(); if (v) location.reload(); },
  };
})(window);
