# Guard + Controller apps (networked version)

Two phone/PC apps watch and control the **same** signal, live. The signal logic is the MASM
program (`platform/dos/`). Nothing else decides anything.

```
 Controller app ─┐                                   ┌─ Guard app(s)
 (browser)       │   WebSocket, pushed on change     │  (phone / tablet / PC)
                 ▼                                   ▼
            ┌──────────── relay/server.js (Node) ────────────┐
            │ forwards bytes, checks role + PIN, no traffic logic │
            └───────────────────────┬────────────────────────┘
                                    │ TCP 127.0.0.1:5000
                         DOSBox  COM1 (serial1=nullmodem)
                                    │ 8250 UART @ 3F8H
                         traffic.exe  ← MASM: te_core.inc + dosnet.inc + hw8255.inc
```

> Android apps: see `docs/mobile.md`.

## 1. What you need
| Item | Why |
|---|---|
| Node.js 18 or newer | runs the relay |
| DOSBox (or DOSBox-X) | runs `traffic.exe` and maps COM1 to TCP |
| MASM 6.x inside DOSBox (optional) | only to rebuild `traffic.exe`; a prebuilt one is included (built with UASM) |

## 2. Run everything
**Windows:** double-click `start.bat` (set `DOSBOX=` if `dosbox` is not on PATH).
**Linux/macOS:** `./start.sh` (`PIN=4321 ./start.sh` to change the PIN).

Or by hand, in two terminals:
```
# terminal 1 - the relay
cd relay
npm install
node server.js --pin 1234

# terminal 2 - DOSBox with COM1 on TCP port 5000
dosbox -conf dosbox-traffic.conf -c "mount c platform/dos" -c "c:" -c "traffic.exe"
```
Start order does not matter: the relay keeps retrying DOSBox every 0.5 s.

The relay prints the addresses. Open:
* Controller (PC):  `http://localhost:8080/controller/`  (asks for the PIN)
* Guard (phone on the same Wi-Fi): `http://<PC-IP>:8080/guard/`

On Windows, allow Node.js through the firewall for private networks the first time.
Phones: "Full screen" in the guard app hides the browser bars.

## 2b. Run with Docker instead (no Node or DOSBox install)
```
docker compose up --build            # PIN=4321 docker compose up --build  to change the PIN
```
Two containers: `dos` (headless DOSBox running the prebuilt `traffic.exe`, COM1 on port 5000, inside the Docker network)
and `relay` (Node, serves the apps on port 8080). Open `http://localhost:8080/controller/` and
`http://<PC-IP>:8080/guard/` as above. The DOS screen is not visible in Docker (no display); the apps are the UI.
`telnet 127.0.0.1 5000` talks to the MASM program directly (only one client at a time, so stop the relay first).
After editing the assembly, rebuild `platform/dos/traffic.exe` (section 3) and run `docker compose up --build` again.

## 3. Rebuild the MASM program (as in the lab)
Inside DOSBox, from `platform/dos`: `build.bat` (MASM 6.x: `ml` + `link`) or `build_tasm.bat`.
Cross-assemble on Linux/macOS: `cd platform/dos && ./build_uasm.sh`.
Run standalone (no network) any time: the program probes COM1 and shows `NET: OFFLINE` if there is none.

## 4. Protocol (ASCII lines, LF-terminated)
MASM → apps: `S <st> <dir> <rem> <dur> <mode> <pause> <seq> <ack> <g> <y> <r>`
(`st` G/Y/R/O, `dir` mask 0-7, `rem` seconds left, `dur` phase length, `mode` A/M, `pause` 0/1,
`seq` publish counter, `ack` last `? n`, `g y r` configured seconds) and `E <1|2>` (1 syntax, 2 range).

Apps → MASM: `G` `Y` `R` set lamp - `X` all off - `Z` reset - `A`/`M` auto/manual - `P1`/`P0` pause/resume -
`D<0-7>` direction mask - `T g y r` durations 1-99 - `? [n]` publish now, echo `n` as `ack`.

You can talk to the MASM program directly: `telnet 127.0.0.1 5000` (DOSBox running), type `R`, press Enter.

## 5. Why it is fast
* MASM publishes the moment something changes (before it redraws the DOS screen) and once per second.
* TCP_NODELAY on every hop, no compression, no polling, one JSON message sent to all apps in one loop.
* The relay is stop-and-wait on the serial line: one command plus its ack request (`? id`) in flight, the next after MASM echoes the id
  (COM1 has no flow control and DOSBox's receive buffer is 16 bytes, so a burst would otherwise lose bytes). Command + ack request go in one TCP write; the controller shows the exact time from click
  to the MASM program's confirmation in its activity log.
* The countdown number is the MASM `rem` value itself, so the DOS screen, guard and controller always show the same digit.

## 6. Safety behaviour
* The signal never depends on the apps: close every browser and the MASM program keeps cycling.
* Guards are read-only (enforced in the relay). Controllers need the PIN. Only 12 command shapes pass the relay and MASM validates them again.
* If the MASM program stops answering for 2.5 s the guard screen dims and says so; if the phone loses Wi-Fi it says "Reconnecting".
* Educational project, not for real road traffic (see `docs/hardware.md`).

## 7. Troubleshooting
| Symptom | Check |
|---|---|
| Relay says "Waiting for DOSBox COM1" forever | DOSBox not running, or `serial1=` line missing; DOSBox prints "nullmodem" and the port at start |
| DOS screen says `NET: OFFLINE` | COM1 not configured: use `-conf dosbox-traffic.conf` |
| Apps say "controller program not responding" | relay is connected but no S lines: check DOSBox window did not stop (press a key if it paused), `cycles=max` |
| Commands feel delayed | keep `txdelay:1 rxdelay:1` in the conf; DOSBox's defaults batch serial data |
| Works on PC, not on phone | same Wi-Fi? firewall? use the IP the relay printed |
| Relay keeps retrying but DOSBox is up | DOSBox accepts ONE client: close any telnet session or second relay |
| Garbled first line | harmless; the relay ignores anything that is not a complete line |
