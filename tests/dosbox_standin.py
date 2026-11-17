#!/usr/bin/env python3
"""
Stand-in for "DOSBox with serial1=nullmodem port:5000": runs the real traffic.exe machine code in a
CPU emulator, in real time, with its COM1 UART wired to a TCP socket. Used to test the relay and the
apps end to end where real DOSBox is not available. NOT a replacement for DOSBox itself.

    python3 tests/dosbox_standin.py platform/dos/traffic.exe [--port 5000]
"""
import os, select, socket, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dosemu import Dos

exe = sys.argv[1]
port = int(sys.argv[sys.argv.index("--port") + 1]) if "--port" in sys.argv else 5000
args = b""
if "--args" in sys.argv:
    args = (" " + sys.argv[sys.argv.index("--args") + 1]).encode()

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(1)
print(f"stand-in listening on {port}", flush=True)

d = Dos(exe, args, clock=lambda: int(time.time()))
conn = None
while d.exit_code is None:
    rl = [srv] + ([conn] if conn else [])
    r, _, _ = select.select(rl, [], [], 0)
    for s in r:
        if s is srv:
            if conn: conn.close()
            conn, _ = srv.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            print("relay connected", flush=True)
        else:
            data = s.recv(4096)
            if not data:
                conn = None
                print("relay disconnected", flush=True)
            else:
                d.feed(data)
    d.run_for_polls(1, count=4000)
    if d.tx and conn:
        try:
            conn.sendall(bytes(d.tx))
        except OSError:
            conn = None
    d.tx.clear()
print("program exited", d.exit_code)
