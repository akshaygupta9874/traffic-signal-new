#!/usr/bin/env python3
"""Headless-Chromium test of both apps against a running relay + MASM program.
   python3 tests/browser_e2e.py [http://localhost:8080] [outdir]"""
import sys, time
from playwright.sync_api import sync_playwright

base = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8080"
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/shots"
fails = 0
def check(c, m):
    global fails
    print(("PASS " if c else "FAIL ") + m)
    if not c: fails += 1

with sync_playwright() as p:
    b = p.chromium.launch()
    gctx = b.new_context(viewport={"width": 390, "height": 780}, device_scale_factor=2, is_mobile=True, has_touch=True)
    cctx = b.new_context(viewport={"width": 1240, "height": 900})
    g = gctx.new_page(); c = cctx.new_page()
    errs = []
    for pg, n in ((g, "guard"), (c, "controller")):
        pg.on("pageerror", lambda e, n=n: errs.append(f"{n}: {e}"))
        pg.on("console", lambda m, n=n: errs.append(f"{n}: {m.text}") if m.type == "error" else None)
    g.goto(base + "/guard/")
    c.goto(base + "/controller/?pin=1234")
    g.wait_for_function("document.getElementById('liveText').textContent==='Live'", timeout=8000)
    c.wait_for_function("document.getElementById('linkText').textContent==='Live'", timeout=8000)
    check(True, "both apps reach Live")
    time.sleep(1.2)
    check("1 guard" in c.inner_text("#chipWatch"), f"controller shows guards watching ({c.inner_text('#chipWatch')})")

    # click RED in the controller, measure until the guard page shows it
    g.evaluate("""() => { window.__t=null; new MutationObserver(()=>{ if(document.body.dataset.state==='R' && !window.__t) window.__t=performance.now(); }).observe(document.body,{attributes:true}); }""")
    c.click('.lampbtn[data-k="R"]')
    g.wait_for_function("document.body.dataset.state==='R'", timeout=3000)
    check(g.inner_text("#word") == "Stop", "guard shows 'Stop' after the controller presses Red")
    c.wait_for_function("document.querySelector('.lampbtn[data-k=R]').getAttribute('aria-pressed')==='true'")
    check(c.inner_text("#word") == "Red", "controller mirror agrees")
    log = c.inner_text("#log")
    check("Red: applied in" in log, f"activity log confirms ({log.splitlines()[0:2]})")

    # direction toggle, pause, timing
    c.click('.dirbtn[data-bit="1"]')
    c.wait_for_function("document.querySelector('.dirbtn[data-bit=\"1\"]').getAttribute('aria-pressed')==='true'")
    check(True, "direction toggle Left lights up in the controller")
    c.click("#bPause")
    c.wait_for_function("document.getElementById('bPause').textContent==='Resume'")
    g.wait_for_function("document.getElementById('mode').textContent==='Paused'")
    check(True, "pause reaches the guard ('Paused')")
    c.click("#bPause")
    c.fill("#tg", "12"); c.fill("#ty", "4"); c.fill("#tr", "9")
    check(not c.is_disabled("#bApply"), "Apply timing enabled after editing")
    c.click("#bApply")
    c.wait_for_function("document.getElementById('timingHint').textContent.includes('12 / 4 / 9')", timeout=3000)
    check(True, "timing 12/4/9 confirmed by the MASM program")
    c.fill("#tg", "10"); c.fill("#ty", "3"); c.fill("#tr", "10"); c.click("#bApply")

    # keyboard like the DOS program
    c.keyboard.press("g")
    g.wait_for_function("document.body.dataset.state==='G'", timeout=3000)
    check(True, "keyboard G in the controller turns the guard screen green")
    c.click("#bOff")
    g.wait_for_function("document.body.dataset.state==='O'", timeout=3000)
    check(g.inner_text("#word") == "Signal off", "all off shows 'Signal off' on the guard")
    c.click("#bReset")
    g.wait_for_function("document.body.dataset.state==='G'", timeout=3000)

    # screenshots
    c.click('.lampbtn[data-k="G"]'); c.click('.dirbtn[data-bit="1"]'); time.sleep(.6)
    g.screenshot(path=f"{out}/guard_phone_green.png")
    c.screenshot(path=f"{out}/controller_desktop.png")
    c.click('.lampbtn[data-k="R"]'); time.sleep(.5)
    g.screenshot(path=f"{out}/guard_phone_red.png")
    g.set_viewport_size({"width": 1280, "height": 720}); time.sleep(.4)
    g.screenshot(path=f"{out}/guard_desktop_red.png")
    g.set_viewport_size({"width": 780, "height": 360}); time.sleep(.4)
    g.screenshot(path=f"{out}/guard_phone_landscape.png")
    g.set_viewport_size({"width": 390, "height": 780})

    # controller phone layout
    pctx = b.new_context(viewport={"width": 390, "height": 844}, device_scale_factor=2, is_mobile=True)
    cp = pctx.new_page(); cp.goto(base + "/controller/?pin=1234"); cp.wait_for_function("document.getElementById('linkText').textContent==='Live'", timeout=8000)
    cp.screenshot(path=f"{out}/controller_phone.png", full_page=True)
    ov = cp.evaluate("document.documentElement.scrollWidth > window.innerWidth")
    check(not ov, "controller has no horizontal scroll on a phone")
    gov = g.evaluate("document.documentElement.scrollWidth > window.innerWidth")
    check(not gov, "guard has no horizontal scroll on a phone")

    # wrong PIN
    w = b.new_context().new_page(); w.goto(base + "/controller/?pin=9999")
    w.wait_for_function("!document.getElementById('gate').hidden", timeout=4000)
    check("not correct" in w.inner_text("#gateErr"), "wrong PIN shows the PIN dialog with an error")

    check(not errs, f"no console errors {errs[:3]}")
    open(f"{out}/.done", "w").write("1")
    b.close()
print("ALL PASSED" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
