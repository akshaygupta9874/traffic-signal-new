#!/bin/sh
# Starts the relay in the background and DOSBox running the MASM traffic program.
# Needs: Node.js 18+ and dosbox (or dosbox-x; set DOSBOX=dosbox-x).
cd "$(dirname "$0")" || exit 1
DOSBOX=${DOSBOX:-dosbox}
[ -d relay/node_modules ] || (cd relay && npm install)
(cd relay && node server.js --pin "${PIN:-1234}") &
RELAY=$!
trap 'kill $RELAY 2>/dev/null' EXIT INT TERM
"$DOSBOX" -conf dosbox-traffic.conf -c "mount c \"$PWD/platform/dos\"" -c "c:" -c "traffic.exe"
