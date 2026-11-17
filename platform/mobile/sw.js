// Offline cache for the mobile PWA. Cache-first for the app shell; bump CACHE when files change.
const CACHE = 'traffic-signal-v1';
const SHELL = ['./', './index.html', './manifest.webmanifest', './icon.svg',
  '../desktop/js/core.js', '../desktop/js/hardware.js', '../desktop/js/renderer.js', '../desktop/js/controller.js'];
self.addEventListener('install', (e) => e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL))));
self.addEventListener('activate', (e) => e.waitUntil(
  caches.keys().then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))));
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(caches.match(e.request).then((hit) => hit || fetch(e.request)));
});
