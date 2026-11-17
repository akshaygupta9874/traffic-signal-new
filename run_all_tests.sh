#!/bin/sh
# Runs every automated check that is possible on this machine; skips the ones whose tools are missing.
cd "$(dirname "$0")"
fail=0
make test || fail=1
node tests/test_core.js || fail=1
sh tests/cross_check.sh || fail=1
if python3 -c "import playwright" 2>/dev/null; then python3 tests/browser_test.py || fail=1; else echo "SKIP browser tests (pip install playwright; playwright install chromium)"; fi
if command -v uasm >/dev/null && python3 -c "import unicorn" 2>/dev/null; then
  (cd platform/dos && uasm -nologo -I. -Icore -Ihw -mz -Fo../../build/traffic.exe main.asm) && python3 tests/dos_emulation_test.py build/traffic.exe || fail=1
else echo "SKIP DOS emulation test (needs uasm and: pip install unicorn pillow)"; fi
[ $fail -eq 0 ] && echo "ALL TEST GROUPS PASSED" || echo "SOME TESTS FAILED"
exit $fail

# --- network link (MASM <-> relay <-> apps) -------------------------------------------------
if command -v uasm >/dev/null 2>&1 && python3 -c "import unicorn" 2>/dev/null; then
  ( cd platform/dos && uasm -nologo -I. -Icore -Ihw -mz -Fo traffic.exe main.asm >/dev/null ) &&
  python3 tests/dos_net_test.py platform/dos/traffic.exe || exit 1
  if [ -d relay/node_modules ]; then
    python3 tests/dosbox_standin.py platform/dos/traffic.exe --port 5000 >/dev/null 2>&1 & SI=$!
    ( cd relay && node server.js --port 8080 >/dev/null 2>&1 ) & RL=$!
    sleep 2; node tests/e2e_ws.js; R=$?
    kill $SI $RL 2>/dev/null; [ $R -eq 0 ] || exit 1
  fi
fi
