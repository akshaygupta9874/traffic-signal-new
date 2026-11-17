# Portability

The 8086 `.EXE` cannot be moved to other platforms; the **design** can. Each target
supplies four things: a clock calling `update(dt)`, an input mapping, a renderer, and a
`HardwareInterface`.

| Target | Core | Clock | Renderer | Output backend | Status in this repo |
|---|---|---|---|---|---|
| DOSBox / real DOS | `te_core.inc` | INT 21H/2CH | `dosrend.inc` (mode 13H) | `hw8255.inc` | built + emulated |
| Desktop simulator | `core.js` | `requestAnimationFrame` | canvas | simulated / 8255 / serial-log | browser-tested |
| Linux CLI / tests | `core/*.cpp` | loop | SVG | 8255 (`/dev/port`), serial, simulated | tested |
| Mobile web (PWA) | `core.js` | `requestAnimationFrame` | canvas, touch buttons | simulated | browser-tested at 390 px |
| Native Android/iOS | `core/*.cpp` via `traffic_c_api` | `Choreographer` / `CADisplayLink` | Canvas / SwiftUI | callback -> BLE/USB | C API tested; Android skeleton provided, not built here |
| Microcontroller | engine on PC, `SerialBackend` | PC | - | UART -> `traffic_signal.ino` | protocol tested; sketch not flashed here |

## Mobile design

```
 UI (Compose / SwiftUI / PWA)  -- draws a TrafficState, forwards taps
        |
 Mobile adapter  (traffic_c_api / JNI / Kotlin TrafficEngine)
        |
 Portable engine (core/*.cpp)  -- owns state & timing
        |
 optional output callback -> Bluetooth/USB LED controller
```

UI requirements met by the PWA: circular lamps, countdown, arrows, zoom buttons,
start/stop (Esc / Exit), reset, configurable timing sheet. Native: add pinch-to-zoom
on a `ViewTransform`-equivalent; the layout constants in `traffic_light_view.hpp` are
the single source of geometry.

Android build: copy `platform/mobile/android/*` and `traffic_c_api.*` into an NDK
module, point `CMakeLists.txt` at `core/`, `System.loadLibrary("traffic")`.
