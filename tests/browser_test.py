#!/usr/bin/env python3
"""Drives the desktop and mobile pages in headless Chromium (Playwright).
    pip install playwright && playwright install chromium
    python3 tests/browser_test.py [--shots outdir]
Checks: no console errors, keyboard + button commands, wheel zoom, timing validation,
backend fallback, and that the canvas really contains lit lamp pixels."""
import os, sys, pathlib
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
shots = sys.argv[sys.argv.index("--shots") + 1] if "--shots" in sys.argv else None
fails = 0


def check(cond, msg):
    global fails
    print(("PASS " if cond else "FAIL ") + msg)
    fails += 0 if cond else 1


def pixel(page, fx, fy):
    return page.evaluate("""([fx, fy]) => { const c = document.getElementById('stage'); const x = c.getContext('2d');
        const d = x.getImageData(Math.round(c.width*fx), Math.round(c.height*fy), 1, 1).data; return [d[0], d[1], d[2]]; }""", [fx, fy])


def run_desktop(p):
    b = p.chromium.launch()
    page = b.new_page(viewport={"width": 1200, "height": 760})
    errors = []
    page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.goto((ROOT / "platform/desktop/index.html").as_uri())
    page.wait_for_timeout(400)
    check(page.inner_text("#sState") == "GREEN", "desktop: starts GREEN")
    check("seconds" in page.inner_text("#sRem"), "desktop: countdown shown")
    page.keyboard.press("r"); page.wait_for_timeout(100)
    check(page.inner_text("#sState") == "RED", "desktop: key R -> RED")
    page.keyboard.press("l"); page.keyboard.press("c")
    d = page.inner_text("#sDir")
    check(d == "STRAIGHT", f"desktop: L (mask 1) then C (cycle +1) -> STRAIGHT (got {d})")
    z0 = page.inner_text("#sZoom")
    page.keyboard.press("+"); page.keyboard.press("+")
    check(page.inner_text("#sZoom") == "1.50x", f"desktop: two + presses -> 1.50x (from {z0})")
    page.keyboard.press("0")
    check(page.inner_text("#sZoom") == "1.00x", "desktop: 0 resets zoom")
    page.mouse.move(400, 300); page.mouse.wheel(0, -300); page.wait_for_timeout(100)
    zoom = float(page.inner_text("#sZoom")[:-1])
    check(zoom > 1.2, f"desktop: mouse wheel zooms in ({zoom:.2f}x)")
    for _ in range(60): page.keyboard.press("+")
    check(page.inner_text("#sZoom") == "3.00x", "desktop: zoom clamps at 3.00x")
    for _ in range(60): page.keyboard.press("-")
    check(page.inner_text("#sZoom") == "0.50x", "desktop: zoom clamps at 0.50x")
    page.keyboard.press("0")
    page.click("[data-cmd=green]"); page.keyboard.press("a")
    check(page.inner_text("#sState") == "GREEN" and page.inner_text("#sDir") == "ALL", "desktop: buttons/keys set GREEN + ALL")
    # lit lamp pixel: green lamp is the lowest lamp (logical y=12 -> centre-relative)
    page.wait_for_timeout(100)
    w = page.evaluate("document.getElementById('stage').clientHeight")
    gy = (w / 2 - 30 + 12) / w
    r, g, bl = pixel(page, 0.5, gy)
    check(g > 150 and r < 120, f"desktop: canvas shows a bright green lamp at its position (rgb {r},{g},{bl})")
    page.keyboard.press("r"); page.wait_for_timeout(100)
    r2, g2, _ = pixel(page, 0.5, gy)
    check(g2 < 90, f"desktop: green lamp dark when RED is active (rgb {r2},{g2})")
    # timing validation
    page.fill("#tG", "0"); page.click("#timing button")
    check("green duration" in page.inner_text("#tErr"), "desktop: invalid timing rejected with message")
    page.fill("#tG", "4"); page.fill("#tY", "2"); page.fill("#tR", "3"); page.click("#timing button")
    check(page.inner_text("#tErr") == "", "desktop: valid timing accepted")
    page.keyboard.press("g")                       # restart green with the new 4 s duration
    page.wait_for_timeout(100)
    check("4 seconds" in page.inner_text("#sRem") or "3 seconds" in page.inner_text("#sRem"), "desktop: new green duration (4 s) used")
    page.wait_for_timeout(4600)
    check(page.inner_text("#sState") in ("YELLOW", "RED"), "desktop: automatic cycle advanced after the new timing")
    # hardware backends
    page.select_option("#backend", "8255"); page.keyboard.press("y"); page.wait_for_timeout(100)
    check("OUT 80H, 02H" in page.inner_text("#portLog"), "desktop: 8255 backend logs 'OUT 80H, 02H' for YELLOW")
    check("PA1" in page.inner_text("#ledY") and "on" in page.get_attribute("#ledY", "class"), "desktop: PA1 LED lit")
    page.select_option("#backend", "8255-missing"); page.wait_for_timeout(100)
    check(page.inner_text("#sOut") == "simulated" and "unavailable" in page.inner_text("#sNote"), "desktop: missing 8255 falls back to simulation")
    # pause / exit
    page.select_option("#backend", "simulated")
    page.keyboard.press("g"); page.keyboard.press(" ")
    t1 = page.inner_text("#sRem"); page.wait_for_timeout(1300)
    check(page.inner_text("#sRem") == t1 and "PAUSED" in page.inner_text("#sMode"), "desktop: Space pauses the countdown")
    page.keyboard.press("Escape")
    check(page.inner_text("#sMode") == "STOPPED" and page.inner_text("#sState") == "OFF", "desktop: Esc stops and darkens all lamps")
    check(not errors, f"desktop: no console errors {errors}")
    if shots:
        os.makedirs(shots, exist_ok=True)
        page.keyboard.press("Enter"); page.keyboard.press("0"); page.keyboard.press("g"); page.keyboard.press("s")
        page.wait_for_timeout(200); page.screenshot(path=f"{shots}/desktop_green_straight.png")
        page.keyboard.press("r"); page.keyboard.press("a"); page.keyboard.press("+"); page.keyboard.press("+"); page.wait_for_timeout(200)
        page.screenshot(path=f"{shots}/desktop_red_all_zoom.png")
    b.close()


def run_mobile(p):
    b = p.chromium.launch()
    ctx = b.new_context(viewport={"width": 390, "height": 844}, device_scale_factor=2, has_touch=True, is_mobile=True)
    page = ctx.new_page()
    errors = []
    page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.goto((ROOT / "platform/mobile/index.html").as_uri())
    page.wait_for_timeout(400)
    check(page.inner_text("#sState") == "GREEN", "mobile: starts GREEN")
    page.tap("[data-cmd=red]"); page.tap("[data-cmd=left]")
    check(page.inner_text("#sState") == "RED" and "LEFT" in page.inner_text("#sLine1"), "mobile: tap RED + LEFT")
    page.tap("[data-cmd=zoomIn]")
    check("1.25x" in page.inner_text("#sLine1"), "mobile: zoom button")
    page.tap("#bMore"); page.wait_for_timeout(100)
    check(page.is_visible("#timing"), "mobile: settings sheet opens")
    page.fill("#tG", "-3"); page.tap("#timing button")
    check(page.inner_text("#tErr") != "", "mobile: invalid timing rejected")
    ok_scroll = page.evaluate("document.documentElement.scrollWidth <= window.innerWidth")
    check(ok_scroll, "mobile: no horizontal scroll at 390px")
    check(not errors, f"mobile: no console errors {errors}")
    if shots:
        page.tap("[data-cmd=green]"); page.tap("[data-cmd=straight]"); page.tap("#bMore"); page.wait_for_timeout(200)
        page.screenshot(path=f"{shots}/mobile_green_straight.png")
    b.close()


with sync_playwright() as p:
    run_desktop(p)
    run_mobile(p)
print("ALL PASSED" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
