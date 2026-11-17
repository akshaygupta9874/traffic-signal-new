#!/usr/bin/env python3
"""Behaviour test of the real DOS machine code (see dosemu.py for the harness)."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dosemu import *
from dosemu import vga_palette

fails = 0


def check(cond, msg):
    global fails
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        fails += 1


def main():
    exe = sys.argv[1]
    png = sys.argv[sys.argv.index("--png") + 1] if "--png" in sys.argv else None

    # 1. default cycle -------------------------------------------------------
    d = Dos(exe).run(36)
    ch = phase_changes(d)
    print("   Port A changes (second, value):", ch[:6])
    check(d.ports.get(0x86) == 0x80, "control word 80H written to port 86H")
    check([v for _, v in ch[:5]] == [1, 2, 4, 1, 2], "sequence GREEN, YELLOW, RED, GREEN, YELLOW on PA0/PA1/PA2")
    t = [s for s, _ in ch[:4]]
    check(t[1] - t[0] == 10 and t[2] - t[1] == 3 and t[3] - t[2] == 10,
          f"phase lengths 10/3/10 s (got {t[1]-t[0]}/{t[2]-t[1]}/{t[3]-t[2]})")
    check(d.mode == 0x13, "graphics mode 13h entered")

    # 2. keys: manual state / direction / zoom / exit --------------------------
    d = Dos(exe)
    d.keys = [(1, ord('R')), (2, ord('l')), (3, ord('+')), (3, ord('+')), (4, ord('A')), (5, ord(' ')), (7, 0x1B)]
    d.run(12)
    check(d.exit_code == 0, "ESC exits with code 0")
    check(d.ports.get(0x80) == 0 and d.ports.get(0x82) == 0, "all lamps/arrows off on exit (PA=0, PB=0)")
    check(d.mode == 3, "text mode restored on exit")
    check(4 in [v for s, p, v in d.port_log if p == 0x80], "R key drove RED (PA2) immediately")
    pb = [v for s, p, v in d.port_log if p == 0x82]
    check(1 in pb and 7 in pb, "L key set LEFT (PB0) and A key set ALL (PB=7)")

    # 3. pause freezes the countdown -------------------------------------------
    d = Dos(exe)
    d.keys = [(2, ord(' '))]
    d.run(20)
    check(len(phase_changes(d)) == 1, "paused at 2 s: no phase change afterwards")

    # 4. command-line timing -----------------------------------------------------
    d = Dos(exe, b" 4 2 3").run(14)
    ch = phase_changes(d)
    check([ch[i + 1][0] - ch[i][0] for i in range(3)] == [4, 2, 3], f"'TRAFFIC 4 2 3' -> 4/2/3 s phases ({ch[:5]})")
    for bad in (b" 0 3 10", b" 10 3", b" a b c", b" 1 2 3 4", b" 100 3 10", b" 12abc 3 4"):
        d = Dos(exe, bad).run(3)
        check(d.exit_code == 1 and "Usage" in d.printed and d.mode == 3,
              f"invalid args {bad!r} -> usage message, exit code 1, no graphics")

    # 5. frames: bright lamp matches state, arrows present, zoom changes size ----
    def lit_rows(fb, colour):                      # rows containing the bright lamp colour near x=160
        return [y for y in range(200) if fb[y * 320 + 160] == colour]

    d = Dos(exe).run(2)
    fb = d.framebuffer()
    check(len(lit_rows(fb, 10)) > 10 and not lit_rows(fb, 12), "GREEN state: green lamp bright, red lamp dark")
    d = Dos(exe)
    d.keys = [(0, ord('R'))]
    d.run(1)
    fb = d.framebuffer()
    check(len(lit_rows(fb, 12)) > 10 and not lit_rows(fb, 10), "RED state (key R): red lamp bright, green lamp dark")
    height = {}
    for name, keys in (("1x", []), ("1.5x", [(0, ord('+'))] * 4), ("0.5x", [(0, ord('-'))] * 4)):
        d = Dos(exe)
        d.keys = list(keys)
        d.run(1)
        fb = d.framebuffer()
        height[name] = len([y for y in range(200) if fb[y * 320 + 160] in (10, 2)])   # green + dark-green rows
    print("   lamp column heights:", height)
    check(height["0.5x"] < height["1x"] < height["1.5x"], "zoom keys scale the drawing (0.5x < 1x < 1.5x)")
    d = Dos(exe)
    d.keys = [(0, ord('C')), (0, ord('C')), (0, ord('C'))]       # STRAIGHT(2) -> 3 -> 4 -> 5 = LEFT+RIGHT
    d.run(1)
    fb = d.framebuffer()
    arrow_y = 88 + 40
    left = sum(1 for x in range(130, 158) if fb[arrow_y * 320 + x] == 10)
    right = sum(1 for x in range(163, 190) if fb[arrow_y * 320 + x] == 10)
    mid = sum(1 for x in range(157, 164) if fb[(arrow_y - 6) * 320 + x] == 10)
    check(left > 3 and right > 3 and mid == 0, "LEFT+RIGHT direction draws left and right arrows, no up arrow")

    if png:
        from PIL import Image
        os.makedirs(png, exist_ok=True)
        scenes = (("dos_green_straight_1x", []),
                  ("dos_red_left_right_1_5x", [(0, ord('R'))] + [(0, ord('C'))] * 3 + [(0, ord('+'))] * 4),
                  ("dos_yellow_all_0_75x", [(0, ord('Y')), (0, ord('A'))] + [(0, ord('-'))] * 2))
        for name, keys in scenes:
            d = Dos(exe)
            d.keys = list(keys)
            d.run(1)
            pal = vga_palette()
            img = Image.new("RGB", (320, 200))
            img.putdata([pal[b] for b in d.framebuffer()])
            img.resize((960, 600), Image.NEAREST).save(f"{png}/{name}.png")
            print("   wrote", f"{png}/{name}.png")
    print("ALL PASSED" if fails == 0 else f"{fails} FAILED")
    sys.exit(1 if fails else 0)


main()
