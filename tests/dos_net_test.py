#!/usr/bin/env python3
"""
Tests the COM1 network protocol of the real assembled traffic.exe (machine code in a CPU
emulator with a simulated 8250 UART). Checks every command, every error reply, the S line
format, the timing, and that the program still works with no serial port at all.

    python3 tests/dos_net_test.py platform/dos/traffic.exe
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dosemu import *

exe = sys.argv[1]
fails = 0


def check(cond, msg):
    global fails
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        fails += 1


def S(line):
    """Parse an S line into a dict."""
    p = line.split()
    assert p[0] == "S" and len(p) == 12, line
    return dict(st=p[1], dir=int(p[2]), rem=int(p[3]), dur=int(p[4]), mode=p[5], pause=int(p[6]),
                seq=int(p[7]), ack=int(p[8]), g=int(p[9]), y=int(p[10]), r=int(p[11]))


def run(script, secs=3, **kw):
    d = Dos(exe, **kw)
    d.rx_script = [(s, c.encode() if isinstance(c, str) else c) for s, c in script]
    d.run(secs)
    return d


def last_state(d):
    return S([l for l in d.tx_lines() if l.startswith("S ")][-1])


# 1. UART programming + first publish -------------------------------------------------
d = run([], 1)
check(d.ports.get(0x3FB) == 0x03 and d.ports.get(0x3F9) == 0, "UART set to 8N1, interrupts off")
first = d.tx_lines()[0]
s = S(first)
print("   first line:", first)
check(s["st"] == "G" and s["rem"] == 10 and s["dur"] == 10 and s["mode"] == "A" and s["pause"] == 0
      and (s["g"], s["y"], s["r"]) == (10, 3, 10) and s["dir"] == 2, "start-up S line = GREEN 10/10 AUTO, config 10/3/10")

# 2. heartbeat each second, countdown, seq increments ----------------------------------
d = run([], 5)
lines = [S(l) for l in d.tx_lines() if l.startswith("S ")]
check([x["rem"] for x in lines][:4] == [10, 9, 8, 7], f"one S line per second with countdown ({[x['rem'] for x in lines][:5]})")
check([x["seq"] for x in lines] == list(range(1, len(lines) + 1)), "seq increments by 1 on every publish")

# 3. state commands, case-insensitive, CR or LF ----------------------------------------
d = run([(1, "r\n"), (2, "Y\r\n"), (3, "g\n")], 4)
st = [S(l)["st"] for l in d.tx_lines() if l.startswith("S ")]
check("R" in st and "Y" in st, "commands R and Y change the state (lower-case, CR/LF accepted)")
check([v for s_, v in porta_timeline(d) if v in (1, 2, 4)][:4] == [1, 4, 2, 1], "8255 Port A follows the app commands (G,R,Y,G)")
check(not any(l.startswith("E") for l in d.tx_lines()), "no error replies for valid commands")

# 4. immediate response in the SAME second (latency path) --------------------------------
d = run([(2, "R\n")], 3)
rows = [S(l) for l in d.tx_lines() if l.startswith("S ")]
r_rows = [x for x in rows if x["st"] == "R"]
check(len(r_rows) >= 1 and r_rows[0]["rem"] == 10 and r_rows[0]["dur"] == 10, "RED command restarts the phase timer (rem=10)")

# 5. direction ---------------------------------------------------------------------------
d = run([(1, "D5\n"), (2, "d0\n")], 3)
dirs = [S(l)["dir"] for l in d.tx_lines() if l.startswith("S ")]
check(5 in dirs and dirs[-1] == 0, "D5 then D0 set the direction mask")
check(5 in [v for s_, p, v in d.port_log if p == 0x82], "direction mask reaches 8255 Port B")
d = run([(1, "D8\n"), (1, "DX\n"), (1, "D\n")], 2)
check(d.tx_lines().count("E 2") == 1 and d.tx_lines().count("E 1") == 2, "D8 -> E 2 (range), DX and bare D -> E 1 (syntax)")

# 6. mode and pause are explicit (idempotent) -----------------------------------------------
d = run([(1, "M\n"), (1, "M\n"), (2, "A\n")], 3)
modes = [S(l)["mode"] for l in d.tx_lines() if l.startswith("S ")]
check(modes[-1] == "A" and "M" in modes, "M, M (again) stays MANUAL, A returns to AUTO")
d = run([(1, "P1\n"), (1, "P1\n")], 15)
check(last_state(d)["pause"] == 1 and len(phase_changes(d)) == 1, "P1 twice stays paused; countdown frozen, no phase change")
d = run([(1, "P1\n"), (3, "P0\n")], 6)
check(last_state(d)["pause"] == 0, "P0 resumes")

# 7. durations ------------------------------------------------------------------------------
d = run([(0, "T 4 2 3\n")], 14)
ch = phase_changes(d)
check(last_state(d)["g"] == 4 and (last_state(d)["y"], last_state(d)["r"]) == (2, 3), "T 4 2 3 updates the configured durations in S")
check(last_state(d)["rem"] <= 10, "T takes effect on the NEXT phase entry (current phase keeps its length)")
d = run([(0, "T 4 2 3\n"), (1, "G\n")], 12)
ch = phase_changes(d)
check([t for t, _ in ch[1:4]] == [5, 7, 10], f"after T 4 2 3, G sent at 1 s: Y at 5 (4 s), R at 7 (2 s), G at 10 (3 s) ({ch[:5]})")
bad = ["T\n", "T 1 2\n", "T 1 2 3 4\n", "T a b c\n", "T 0 3 10\n", "T 100 3 10\n", "T 99999999 1 1\n", "T 5 0 5\n"]
d = run([(1, b) for b in bad], 3)
errs = [l for l in d.tx_lines() if l.startswith("E")]
check(errs == ["E 1", "E 1", "E 1", "E 1", "E 2", "E 2", "E 2", "E 2"], f"invalid T commands rejected with E1/E2 ({errs})")
check(last_state(d)["g"] == 10 and last_state(d)["y"] == 3 and last_state(d)["r"] == 10,
      "rejected T commands changed nothing (all-or-nothing validation)")

# 8. all off / reset ----------------------------------------------------------------------------
d = run([(1, "X\n"), (4, "Z\n")], 6)
sts = [S(l) for l in d.tx_lines() if l.startswith("S ")]
check(any(x["st"] == "O" for x in sts) and sts[-1]["st"] == "G" and sts[-1]["rem"] >= 9, "X = all off (O), Z = reset to GREEN full time")
check(0 in [v for s_, v in porta_timeline(d)], "all off writes 0 to Port A")
off_rows = [x for x in sts if x["st"] == "O"]
check(len(set(x["rem"] for x in off_rows)) == 1, "no countdown while OFF")

# 9. ack echo (used by the relay to measure latency) -----------------------------------------------
d = run([(1, "? 4242\n")], 2)
check(any(S(l)["ack"] == 4242 for l in d.tx_lines() if l.startswith("S ")), "'? 4242' publishes immediately and echoes ack=4242")
d = run([(1, "G\n? 65535\n")], 2)
check(last_state(d)["ack"] == 65535, "command and ack in the same burst -> one consistent S line with ack=65535")

# 10. garbage and robustness --------------------------------------------------------------------------
junk = ["HELLO\n", "GG\n", "G 1\n", "\n\n\n", "!!!\n", "A" * 200 + "\n", "\x00\x01\x02\n"]
d = run([(1, j) for j in junk], 3)
check(last_state(d)["st"] in "GYR", "garbage input never crashes or corrupts the state machine")
check(d.exit_code is None, "program still running after garbage")
check(d.tx_lines().count("E 1") >= 5, "garbage answered with E 1")

# 11. keyboard and app share one path -----------------------------------------------------------------------
d = Dos(exe)
d.keys = [(2, ord('R'))]
d.rx_script = [(4, b"G\n")]
d.run(6)
sts = [S(l)["st"] for l in d.tx_lines() if l.startswith("S ")]
check("R" in sts and sts[-1] == "G", "keyboard R then app G: both are published, no divergence")

# 12. exit publishes OFF ---------------------------------------------------------------------------------------
d = Dos(exe)
d.keys = [(2, 0x1B)]
d.run(5)
check(d.exit_code == 0 and S(d.tx_lines()[-1])["st"] == "O", "ESC: final S line says OFF (apps show 'controller stopped')")

# 13. no UART present = old behaviour ---------------------------------------------------------------------------
d = Dos(exe, uart=False).run(14)
check(len(d.tx) == 0 and [v for _, v in phase_changes(d)][:3] == [1, 2, 4], "without a serial port the program runs normally and sends nothing")

print("ALL PASSED" if fails == 0 else f"{fails} FAILED")
sys.exit(1 if fails else 0)
