#!/usr/bin/env python3
"""
dosemu.py - shared CPU-emulator harness (Unicorn) for traffic.exe, with a simulated 8250 UART at 3F8H.
(Original description of the test that uses it:)
Runs the assembled DOS program (traffic.exe) inside a CPU emulator (Unicorn) with
INT 10h / 16h / 21h and the 8255 ports stubbed, and checks the behaviour:
  * the exact bytes written to the 8255 (Port A / Port B / control word 80H)
  * phase lengths 10 s / 3 s / 10 s and the order GREEN -> YELLOW -> RED -> GREEN
  * manual keys, pause, command-line timing, invalid command line
  * a rendered frame (framebuffer -> PNG) when --png is given

This executes the real machine code but is not a replacement for DOSBox.
    pip install unicorn pillow
    python3 tests/dos_emulation_test.py path/to/traffic.exe [--png outdir]
"""
import os
import struct
import sys

from unicorn import *
from unicorn.x86_const import *

PSP, LOAD = 0x0FF0, 0x1000
CLOCK_POLLS_PER_SECOND = 60          # INT 21h/2Ch calls per simulated second


def vga_palette():
    base16 = [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170), (170, 0, 0), (170, 0, 170), (170, 85, 0),
              (170, 170, 170), (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255), (255, 85, 85),
              (255, 85, 255), (255, 255, 85), (255, 255, 255)]
    pal = base16 + [(0, 0, 0)] * 240
    for idx, (r, g, b) in {2: (0, 14, 3), 4: (16, 2, 2), 6: (20, 14, 0)}.items():   # DAC set in GFX_Init (6-bit)
        pal[idx] = tuple(round(v * 255 / 63) for v in (r, g, b))
    return pal


class Dos:
    def __init__(self, exe_path, cmdtail=b"", uart=True, clock=None):
        self.uart = uart               # False = no serial port at 3F8H (DOSBox without serial1=)
        self.clock = clock             # callable -> seconds (real-time mode); None = simulated
        self.rx = bytearray()          # bytes "sent by the apps" waiting in the UART
        self.tx = bytearray()          # bytes the program wrote to the UART
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, 0x100000)
        self.keys = []                 # (second, ascii) pending keystrokes
        self.polls = 0
        self.target = 0
        self.rx_script = []            # (second, bytes) delivered when that second is reached
        self.port_log = []             # (second, port, value)
        self.text = []                 # (row, col, char) teletype output of the current frame
        self.exit_code = None
        self.mode = 3
        self.printed = ""
        self.ports = {}
        self.cx = self.cy = 0
        data = open(exe_path, "rb").read()
        (_, _, _, crlc, cparhdr, _, _, ss, sp, _, ip, cs, lfarlc) = struct.unpack("<13H", data[:26])
        image = bytearray(data[cparhdr * 16:])
        for i in range(crlc):                      # apply relocations
            off, seg = struct.unpack("<2H", data[lfarlc + 4 * i: lfarlc + 4 * i + 4])
            pos = seg * 16 + off
            w = struct.unpack("<H", image[pos:pos + 2])[0]
            image[pos:pos + 2] = struct.pack("<H", (w + LOAD) & 0xFFFF)
        uc.mem_write(LOAD * 16, bytes(image))
        uc.mem_write(PSP * 16 + 0x80, bytes([len(cmdtail)]) + cmdtail + b"\r")
        for reg, v in ((UC_X86_REG_CS, cs + LOAD), (UC_X86_REG_SS, ss + LOAD),
                       (UC_X86_REG_DS, PSP), (UC_X86_REG_ES, PSP)):
            uc.reg_write(reg, v)
        uc.reg_write(UC_X86_REG_SP, sp)
        self.ip = ip
        self.cs = cs + LOAD
        uc.hook_add(UC_HOOK_INTR, self.on_intr)
        uc.hook_add(UC_HOOK_INSN, self.on_out, None, 1, 0, UC_X86_INS_OUT)
        uc.hook_add(UC_HOOK_INSN, self.on_in, None, 1, 0, UC_X86_INS_IN)

    @property
    def second(self):
        if self.clock:
            return self.clock()
        return self.polls // CLOCK_POLLS_PER_SECOND

    def feed(self, data):
        self.rx += data if isinstance(data, bytes) else data.encode()

    def tx_lines(self):
        return [l for l in self.tx.decode().split("\n") if l]

    def on_in(self, uc, port, size, _):
        if 0x3F8 <= port <= 0x3FF and self.uart:
            if port == 0x3F8:
                return self.rx.pop(0) if self.rx else 0
            if port == 0x3FD:                      # LSR: THRE+TEMT always, DR when data waits
                return 0x60 | (1 if self.rx else 0)
        return self.ports.get(port, 0xFF)

    def on_out(self, uc, port, size, value, _):
        if port == 0x3F8 and self.uart and not (self.ports.get(0x3FB, 0) & 0x80):
            self.tx.append(value & 0xFF)           # THR write (DLAB=0)
            return
        self.ports[port] = value & 0xFF
        if 0x3F8 <= port <= 0x3FF:
            return
        self.port_log.append((self.second, port, value & 0xFF))

    def set_zf(self, zf):
        fl = self.uc.reg_read(UC_X86_REG_EFLAGS)
        self.uc.reg_write(UC_X86_REG_EFLAGS, (fl | 0x40) if zf else (fl & ~0x40))

    def on_intr(self, uc, vec, _):
        ax = uc.reg_read(UC_X86_REG_AX)
        dx = uc.reg_read(UC_X86_REG_DX)
        ah, al = ax >> 8, ax & 0xFF
        if vec == 0x10:
            if ah == 0x00:
                self.mode = al
            elif ah == 0x02:
                self.cy, self.cx = dx >> 8, dx & 0xFF
            elif ah == 0x0E:
                if (self.cy, self.cx) == (0, 0):
                    self.text = []                   # the title starts every frame
                self.text.append((self.cy, self.cx, chr(al)))
                self.cx += 1
        elif vec == 0x16:
            due = [k for k in self.keys if k[0] <= self.second]
            if ah == 0x01:
                if due:
                    uc.reg_write(UC_X86_REG_AX, due[0][1])
                    self.set_zf(False)
                else:
                    self.set_zf(True)
            elif ah == 0x00 and due:
                self.keys.remove(due[0])
                uc.reg_write(UC_X86_REG_AX, due[0][1])
        elif vec == 0x21:
            if ah == 0x2C:
                self.polls += 1
                while self.rx_script and self.rx_script[0][0] <= self.second:
                    self.feed(self.rx_script.pop(0)[1])
                uc.reg_write(UC_X86_REG_DX, (self.second % 60) << 8)
                # 2Ch is called at the top of the main loop, i.e. between frames:
                # a safe place to stop and look at the screen / ports.
                if self.second >= self.target and not self.keys:
                    uc.emu_stop()
            elif ah == 0x09:
                a, s = (uc.reg_read(UC_X86_REG_DS) << 4) + dx, b""
                while bytes(uc.mem_read(a, 1)) != b"$":
                    s += bytes(uc.mem_read(a, 1))
                    a += 1
                self.printed += s.decode()
            elif ah == 0x4C:
                self.exit_code = al
                uc.emu_stop()

    def run_for_polls(self, n_slices=1, count=200_000):
        """Real-time mode: execute a slice of instructions and return (used by the TCP stand-in)."""
        self.target = 1 << 60
        for _ in range(n_slices):
            self.uc.emu_start(self.cs * 16 + self.ip, 0, count=count)
            self.ip = self.uc.reg_read(UC_X86_REG_IP)
            self.cs = self.uc.reg_read(UC_X86_REG_CS)
            if self.exit_code is not None:
                break

    def run(self, seconds):
        """Run until the program exits, or `seconds` of simulated time have passed
        (and every scripted key has been consumed), stopping between frames."""
        self.target = seconds
        while self.exit_code is None and not (self.second >= seconds and not self.keys):
            start = self.cs * 16 + self.ip
            try:
                self.uc.emu_start(start, 0, count=2_000_000)
            except UcError as e:
                u = self.uc
                cs_, ip_ = u.reg_read(UC_X86_REG_CS), u.reg_read(UC_X86_REG_IP)
                code = bytes(u.mem_read(cs_ * 16 + ip_, 8)).hex()
                raise SystemExit(f"CPU fault: {e} at {cs_:04X}:{ip_:04X} bytes {code}")
            self.ip = self.uc.reg_read(UC_X86_REG_IP)
            self.cs = self.uc.reg_read(UC_X86_REG_CS)
        return self

    def framebuffer(self):
        return bytes(self.uc.mem_read(0xA0000, 64000))


def porta_timeline(d):
    return [(s, v) for s, p, v in d.port_log if p == 0x80]


def phase_changes(d):
    out = []
    for s, v in porta_timeline(d):
        if v in (1, 2, 4) and (not out or out[-1][1] != v):
            out.append((s, v))
    return out



def porta_timeline(d):
    return [(s, v) for s, p, v in d.port_log if p == 0x80]


def phase_changes(d):
    out = []
    for s, v in porta_timeline(d):
        if v in (1, 2, 4) and (not out or out[-1][1] != v):
            out.append((s, v))
    return out
