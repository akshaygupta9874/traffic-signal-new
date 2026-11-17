# Architecture

## 1. What the original program is

`legacy/original_traffic.asm` is a **DOS application that talks to the 8255 directly**
with `OUT 80H, AL`. It is *not* a device driver in the modern sense: there is no
driver model, no abstraction, no OS-level hardware arbitration. It works because DOS
lets any program execute `IN`/`OUT`, and because the programmer knows which ports the
8255 is wired to. Consequently the **same `.EXE` cannot run on Android, Windows or
Linux** (no DOS, no 16-bit mode, no raw port access) and cannot drive other hardware.
What *can* be portable is the **logic** (the G -> Y -> R cycle, timings, countdown,
direction model) - so that is what this project separates out.

### Limits of the original design

| Limitation | Consequence |
|---|---|
| Timing is `MOV CX,10 ... LOOP` around a blocking 1-s delay | keys only polled once a second; durations hard-coded |
| State, drawing, port I/O and DOS calls live in the same procedures (`SHOW_GREEN` does `OUT`-adjacent work, drawing, text, cursor) | nothing can be reused or tested alone |
| Text-mode blocks (`0DBH`) | no circles, no scaling, no arrows |
| Any key exits | no manual control |
| Port addresses inlined in `MAIN` | cannot swap the output device |

## 2. Target architecture

```
                       TRAFFIC LIGHT CORE
                              |
            +-----------------+-----------------+
            |                                   |
      Traffic engine                       Configuration
  (state machine, timers,                (durations, zoom,
   direction, manual ops)                 direction, mode)
            |
            v
      TrafficState  { state, direction, remainingTime, duration }
            |                       (read-only snapshot)
   +--------+------------------------+
   |                                 |
   v                                 v
 HardwareInterface                 Renderer(s)
   |-- I8255Backend  -> OUT 80H/82H    |-- DOS mode-13h renderer (asm)
   |-- SerialBackend -> USB/UART -> MCU|-- Canvas renderer (desktop/mobile web)
   |-- SimulatedHardware (virtual LEDs)|-- SVG renderer (tests, docs)
   '-- CallbackHardware (mobile C API) '-- Android Canvas / SwiftUI (future)
```

The core knows `GREEN/YELLOW/RED/OFF`, `direction`, `remainingTime`, `duration`.
It does **not** know `INT 10H`, `INT 21H`, DOSBox, port numbers, Canvas, or GPIO.

### Implementations of the core

The core exists three times, because the platforms cannot share a binary:

| Language | Location | Used by |
|---|---|---|
| C++17 | `core/` | tests, CLI demo, Linux hardware, mobile C API / JNI |
| JavaScript | `platform/desktop/js/core.js` | desktop simulator, mobile PWA |
| 8086 assembly | `platform/dos/core/te_core.inc` | DOS / DOSBox / real 8255 |

They are the same state machine. `tests/cross_check.sh` runs the C++ and JS engines
through identical scripted timelines and requires byte-identical traces; the DOS engine
is verified by executing the real machine code in a CPU emulator
(`tests/dos_emulation_test.py`).

## 3. Layers and files

```
core/            direction, traffic_state, traffic_config, traffic_engine   (no I/O)
hardware/        hardware_interface, i8255_backend, serial_backend,
                 simulated_hardware, hardware_select, dev_port_io, arduino/
rendering/       renderer_interface, view_transform (zoom), traffic_light_view,
                 svg_renderer
platform/dos/    main.asm + modules (core/, hw/, dostimer, dosinput, dosgfx, dosrend, doscfg)
platform/desktop/ browser simulator (JS port of core + renderers)
platform/mobile/ PWA, C API (traffic_c_api.*), Android JNI/Kotlin skeleton
tests/           C++, JS, C, browser, DOS-emulation, cross-check
legacy/          original_traffic.asm (untouched)
```

### DOS module responsibilities

| File | Role | Replaces in original |
|---|---|---|
| `core/te_core.inc` | state machine (`TE_Tick`, `TE_SetState`...) | the `GREEN:`/`YELLOW:`/`RED:` blocks + `MOV CX,n / LOOP` |
| `hw/hw8255.inc` | **only** file with ports 80H/82H/86H | `MOV DX,80H / OUT` in `MAIN`, `EXIT_PROGRAM` |
| `dostimer.inc` | INT 21H/2CH clock, non-blocking | `DELAY_1SEC` |
| `dosinput.inc` | INT 16H keys | `CHECK_KEY` |
| `dosgfx.inc` | mode 13H pixels, circle, rect, BIOS text | `PRINT_COLOR`, `PRINT_WHITE`, `CLEAR_SCREEN` |
| `dosrend.inc` | housing, lamps, arrows, countdown, zoom | `SHOW_*`, `DRAW_SIGNAL`, `DRAW_GAP`, `DRAW_BULB`, `BULB_ROW`, `SAVE_POS`, `PRINT_NUMBER` |
| `doscfg.inc` | command-line durations | (new) |
| `main.asm` | event loop + key dispatch | `MAIN`, `EXIT_PROGRAM` |

## 4. What was preserved / refactored / added

**Preserved:** `.MODEL SMALL`, `.STACK`, `.DATA`, `.CODE`; the 8255 map
(A=80H, B=82H, C=84H, control=86H, control word 80H, PA0=G, PA1=Y, PA2=R, i.e.
`01H/02H/04H`); G->Y->R->G order and 10/3/10 default timing; `INT 10H`, `INT 16H`,
`INT 21H` (clock, exit, messages); all lamps off on exit; the DOS-seconds-field timing
method; the "dark version of each colour when off" idea (`*_OFF` attributes ->
`lamp_off[]`).

**Refactored** (same idea, new home): table above. Hard-coded `MOV CX,10` loops became
`te_dur[]` + `TE_Tick`; `DELAY_1SEC`'s busy-wait became the non-blocking `DT_PollSecond`
so keys react immediately; `PRINT_NUMBER` + `SAVE_POS` became `RD_Countdown`, which
only *reads* `te_remain`; `EXIT_PROGRAM` became `HW_Shutdown` + `GFX_Exit`.

**Added:** circular lamps, scalable logical coordinate system and zoom, direction
model and arrows, manual keys, pause, auto/manual mode, command-line timing with
validation, Port B arrow outputs, hardware probe, and every non-DOS platform.

**Why:** each change removes one coupling from section 1.

## 5. Zoom

Geometry is stored once, in logical units (`traffic_light_view.hpp::layout`,
`dosrend.inc` constants). One transform converts:

```
screenX = originX + logicalX * scale      radius = logicalRadius * scale
```

C++/JS use a floating-point `scale` (0.5..3.0, step 0.25). DOS uses 8.8 fixed point
(`rd_scale`, 0.5..1.5, step 0.125 - a 200-pixel screen cannot show more). Rectangle
edges are scaled individually so neighbouring shapes never gain gaps. Housing, lamps,
pole, arrows, and countdown all go through the same transform.

## 6. Directions

`Direction` is a 3-bit mask (LEFT=1, STRAIGHT=2, RIGHT=4) held **beside** the colour,
never inside it: `GREEN + LEFT` is two independent fields. The engine stores/validates
the mask; each renderer chooses how to show it (polygons on canvas/SVG, rectangle-built
glyphs in DOS); the 8255 backend writes the same mask to Port B (PB0/1/2), and the
serial backend sends it in the protocol line. `NONE` gives a plain three-lamp signal.

## 7. Error handling

| Situation | Behaviour |
|---|---|
| invalid durations (config, CLI, UI) | rejected with message; engine falls back to defaults; DOS prints usage, exits 1 |
| invalid zoom | rejected / clamped; previous scale kept |
| invalid direction/state value | `false`/CF=1 returned, state unchanged |
| negative/NaN time delta | ignored |
| hardware missing / port unopenable / 8255 probe fails | `selectHardware` falls back to `SimulatedHardware` and reports why |
| unsupported platform for raw ports | `DevPortIO` stays closed -> simulation |
| unknown key | ignored |
