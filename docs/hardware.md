# Hardware

> **Educational prototype only.** This software has no fault detection, conflict
> monitor, watchdog certification, or fail-safe wiring. It is **not** safe, tested, or
> certified for public-road traffic control (which requires conflict monitoring,
> certified controllers, and compliance with national standards). Do not connect it to
> signals that road users rely on.

## Path A - 8255 board (the original)

```
 PC/DOS program --OUT--> 8255 --PA0..PA2--> driver (ULN2003/transistor) --> G / Y / R LEDs
                                  PB0..PB2--> driver --> left / straight / right arrow LEDs
```

## Path B - microcontroller prototype

```
 PC (TrafficEngine + SerialBackend)
   | USB serial, 9600 8N1, "T <G|Y|R|O> <mask 0..7>\n"
   v
 Arduino (hardware/arduino/traffic_signal.ino)
   D2 RED  D3 YELLOW  D4 GREEN  D5 LEFT  D6 STRAIGHT  D7 RIGHT
   each LED -> ~330 ohm -> GND
```

* The sketch validates every line (replies `OK`/`ERR`) and **blanks all outputs if no
  valid line arrives for 5 s**, so a dead link never leaves a lamp latched.
* `SerialBackend` resends the current line at least once per second as a heartbeat.
* Prototype safety: one lamp at a time only comes from software; a hardware interlock
  would be needed for anything beyond a demo.

## Choosing a backend at run time

`selectHardware(preferred, fallback)` initialises the preferred backend; if it is not
open/available the simulated backend is used and a note is returned. Examples:
8255 on Linux needs `DevPortIO` (root); serial needs a device path
(`FileByteSink("/dev/ttyUSB0")`).
