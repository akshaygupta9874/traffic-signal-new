#!/usr/bin/env node
/*
 * make-apk.js - builds the Guard and Controller Android apps (Capacitor wrapper around ../apps).
 *
 *   node mobile/make-apk.js both            -> mobile/out/signal-guard.apk and signal-controller.apk
 *   node mobile/make-apk.js guard
 *   node mobile/make-apk.js controller
 *   node mobile/make-apk.js both --no-gradle    (prepare the Android projects only)
 *
 * Needs: Node.js 20+, JDK 21, Android SDK (ANDROID_HOME). Easiest: Android Studio installs the last two.
 * The result is a debug-signed APK: install it on a phone after allowing "install unknown apps".
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const APPS = path.join(ROOT, 'apps');
const BUILD = path.join(__dirname, 'build');
const OUT = path.join(__dirname, 'out');
const win = process.platform === 'win32';
const CAP = '^7.6.0';

const ROLES = {
  guard:      { id: 'com.trafficsignal.guard',      name: 'Signal Guard' },
  controller: { id: 'com.trafficsignal.controller', name: 'Signal Controller' },
};

const args = process.argv.slice(2);
const which = args.find((a) => !a.startsWith('--')) || 'both';
const noGradle = args.includes('--no-gradle');
const roles = which === 'both' ? Object.keys(ROLES) : [which];
if (roles.some((r) => !ROLES[r])) { console.error('Usage: node mobile/make-apk.js guard|controller|both [--no-gradle]'); process.exit(2); }

function run(cmd, argv, cwd) {
  console.log(`\n> ${cmd} ${argv.join(' ')}   (${path.relative(ROOT, cwd)})`);
  const r = spawnSync(cmd, argv, { cwd, stdio: 'inherit', shell: win });
  if (r.status !== 0) { console.error(`\nFailed: ${cmd} ${argv.join(' ')}`); process.exit(r.status || 1); }
}
function copyDir(src, dst) {
  fs.mkdirSync(dst, { recursive: true });
  for (const e of fs.readdirSync(src, { withFileTypes: true })) {
    const s = path.join(src, e.name), d = path.join(dst, e.name);
    e.isDirectory() ? copyDir(s, d) : fs.copyFileSync(s, d);
  }
}

if (!noGradle) {
  if (!process.env.ANDROID_HOME && !process.env.ANDROID_SDK_ROOT) {
    console.error('ANDROID_HOME is not set. Install Android Studio (it installs the SDK), then set ANDROID_HOME, e.g.\n  Windows: C:\\Users\\<you>\\AppData\\Local\\Android\\Sdk\n  macOS:   ~/Library/Android/sdk\n  Linux:   ~/Android/Sdk\nOr run with --no-gradle and open mobile/build/<role>/android in Android Studio.');
    process.exit(1);
  }
}

for (const role of roles) {
  const { id, name } = ROLES[role];
  const dir = path.join(BUILD, role);
  console.log(`\n=== ${name} (${id}) ===`);
  fs.rmSync(dir, { recursive: true, force: true });
  fs.mkdirSync(path.join(dir, 'www'), { recursive: true });

  fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({
    name: 'signal-' + role, version: '1.0.0', private: true,
    dependencies: { '@capacitor/core': CAP, '@capacitor/android': CAP },
    devDependencies: { '@capacitor/cli': CAP },
  }, null, 2));
  fs.writeFileSync(path.join(dir, 'capacitor.config.json'), JSON.stringify({
    appId: id, appName: name, webDir: 'www',
    // The pages are bundled in the app; they reach the relay over plain ws:// on the local network.
    server: { androidScheme: 'http', cleartext: true },
    android: { allowMixedContent: true },
  }, null, 2));

  // Web files: the role's page becomes index.html; its "../common/" links become "common/".
  const html = fs.readFileSync(path.join(APPS, role, 'index.html'), 'utf8').replace(/\.\.\/common\//g, 'common/');
  fs.writeFileSync(path.join(dir, 'www', 'index.html'), html);
  copyDir(path.join(APPS, 'common'), path.join(dir, 'www', 'common'));

  run('npm', ['install', '--no-audit', '--no-fund'], dir);
  run('npx', ['cap', 'add', 'android'], dir);
  run('npx', ['cap', 'sync', 'android'], dir);

  // Android 9+ blocks plain-text traffic (ws://, http://) unless the manifest allows it. The relay runs on the
  // local network without TLS, so allow it, and make sure the app may use the network.
  const manifest = path.join(dir, 'android', 'app', 'src', 'main', 'AndroidManifest.xml');
  let mx = fs.readFileSync(manifest, 'utf8');
  if (!/usesCleartextTraffic/.test(mx)) mx = mx.replace('<application', '<application\n        android:usesCleartextTraffic="true"');
  if (!/permission\.INTERNET/.test(mx)) mx = mx.replace('</manifest>', '    <uses-permission android:name="android.permission.INTERNET" />\n</manifest>');
  fs.writeFileSync(manifest, mx);

  if (noGradle) { console.log(`Prepared: ${path.relative(ROOT, path.join(dir, 'android'))}`); continue; }
  run(win ? 'gradlew.bat' : './gradlew', ['assembleDebug'], path.join(dir, 'android'));
  const apk = path.join(dir, 'android', 'app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk');
  fs.mkdirSync(OUT, { recursive: true });
  const target = path.join(OUT, `signal-${role}.apk`);
  fs.copyFileSync(apk, target);
  console.log(`\nBuilt ${path.relative(ROOT, target)}`);
}
