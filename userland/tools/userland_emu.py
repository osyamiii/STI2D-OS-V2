#!/usr/bin/env python3
"""Boot the real STI2D-OS userland (userland.allow3rdparty.B.elf) in the Unicorn
Cortex-M emulator, with the NumWorks kernel's system calls (SVC) serviced here.

This is not the calculator: the kernel, the screen controller and the
keyboard matrix are modelled at the level of the system calls the userland
makes. It runs the userland's own code end to end (crt0, Ion::Init, Escher,
AppsContainer, the STI2D-OS shell, Epsilon's apps) and records the screen, so
start-up, the home screen, the keys and the app transitions can be checked
before flashing a real calculator.

  userland_emu.py ELF --ms 6000 --keys "3000-3100:ok" --shots 2500,5000 --out DIR

Keys: name or key index, held from a to b ms (see KEYS). Exit status 0 when the
run reached the end without a CPU fault or an unhandled memory access.
"""
import argparse
import os
import struct
import sys

from elftools.elf.elffile import ELFFile
from unicorn import (UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_INTR, UC_HOOK_MEM_INVALID, UC_MODE_MCLASS, UC_MODE_THUMB, Uc,
                     UcError)
from unicorn.arm_const import (UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                               UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC,
                               UC_CPU_ARM_CORTEX_M7)

KEYS = {"left": 0, "up": 1, "down": 2, "right": 3, "ok": 4, "back": 5, "home": 6, "onoff": 8, "shift": 12,
        "alpha": 13, "xnt": 14, "var": 15, "toolbox": 16, "backspace": 17, "exp": 18, "ln": 19, "log": 20,
        "i": 21, "comma": 22, "power": 23, "sin": 24, "cos": 25, "tan": 26, "pi": 27, "sqrt": 28, "square": 29,
        "7": 30, "8": 31, "9": 32, "lparen": 33, "rparen": 34, "4": 36, "5": 37, "6": 38, "mul": 39, "div": 40,
        "1": 42, "2": 43, "3": 44, "plus": 45, "minus": 46, "0": 48, "dot": 49, "ee": 50, "ans": 51, "exe": 52}

FLASH, FLASH_SIZE = 0x90000000, 0x800000
SRAM, SRAM_SIZE = 0x24000000, 0x40000
KERNEL_HEADER_B = 0x90400000 + 8
INTERNAL_FLASH, INTERNAL_FLASH_SIZE = 0x08000000, 0x80000
CPU_HZ = 550_000_000
TRAMPOLINE = 0x0807FC00
TRAMPOLINE_CODE = 0x08001000
EVENT_NONE = 216


def epsilon_crc32(data):
    """Ion::crc32Byte (CRC-32/MPEG-2 fed word by word, as tools/emu.py)."""
    crc = 0xFFFFFFFF

    def eat(crc, b):
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) if crc & 0x80000000 else crc << 1
            crc &= 0xFFFFFFFF
        return crc

    n = len(data) // 4
    for i in range(n):
        for j in (3, 2, 1, 0):
            crc = eat(crc, data[4 * i + j])
    for b in data[4 * n:]:
        crc = eat(crc, b)
    return crc


class Userland:
    def __init__(self, elf_path, version, keys, trace_svc=False):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        uc = self.uc
        uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
        uc.reg_write(UC_ARM_REG_C1_C0_2, 0xF << 20)  # CPACR: full access to the FPU
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        uc.mem_map(FLASH, FLASH_SIZE)
        uc.mem_write(FLASH, b"\xff" * FLASH_SIZE)
        uc.mem_map(SRAM, SRAM_SIZE)
        uc.mem_map(INTERNAL_FLASH, INTERNAL_FLASH_SIZE)
        uc.mem_map(0x20000000, 0x20000)  # DTCM
        with open(elf_path, "rb") as f:
            elf = ELFFile(f)
            for seg in elf.iter_segments():
                if seg["p_type"] != "PT_LOAD" or not seg["p_filesz"]:
                    continue
                addr = seg["p_paddr"]
                if FLASH <= addr < FLASH + FLASH_SIZE:
                    uc.mem_write(addr, seg.data())
            self.symbols = {}
            for sec in elf.iter_sections():
                if sec.name == ".symtab":
                    for s in sec.iter_symbols():
                        if s.name:
                            self.symbols[s.name] = s["st_value"]
        header = (struct.pack("<I", 0xDEC00DF0) + version.encode().ljust(8, b"\0") + b"emulated" +
                  struct.pack("<I", 0xDEC00DF0))
        uc.mem_write(KERNEL_HEADER_B, header)
        origin = self.symbols["USERLAND_ORIGIN"]
        sp, reset = struct.unpack("<II", uc.mem_read(origin + 48, 8))
        uc.reg_write(UC_ARM_REG_SP, sp)
        self.pc = reset | 1
        self.screen = bytearray(320 * 240 * 2)
        self.keys = keys
        self.now_ms = 0.0
        self.trace_svc = trace_svc
        self.svc_counts = {}
        self.unhandled = {}
        self.faults = []
        self.prev_state = 0
        self.rng = 0x12345678
        self.shift_alpha = 0
        self.resets = 0
        self.locked = 0
        # The bootloader's trampoline table (TrampolineOrigin, internal flash):
        # the C library functions the userland calls through it, run here.
        for i in range(13):
            uc.mem_write(TRAMPOLINE + 4 * i, struct.pack("<I", (TRAMPOLINE_CODE + 16 * i) | 1))
        uc.mem_write(TRAMPOLINE_CODE, b"\x70\x47" * (8 * 13))  # bx lr (never reached)
        uc.hook_add(UC_HOOK_CODE, self._trampoline, begin=TRAMPOLINE_CODE, end=TRAMPOLINE_CODE + 16 * 13)
        uc.hook_add(UC_HOOK_INTR, self._intr)
        uc.hook_add(UC_HOOK_MEM_INVALID, self._invalid)

    # ---------------------------------------------------------------- state
    def keyboard_state(self):
        s = 0
        for a, b, k in self.keys:
            if a <= self.now_ms < b:
                s |= 1 << k
        return s

    def reg(self, r):
        return self.uc.reg_read(r)

    def save_png(self, path):
        from PIL import Image
        import numpy as np
        a = np.frombuffer(bytes(self.screen), dtype="<u2").reshape(240, 320)
        rgb = np.stack([((a >> 11) & 31) * 255 // 31, ((a >> 5) & 63) * 255 // 63, (a & 31) * 255 // 31], -1)
        Image.fromarray(rgb.astype("uint8"), "RGB").save(path)

    # ---------------------------------------------------------------- hooks
    def _cstr(self, a, limit=1 << 16):
        out = bytearray()
        while len(out) < limit:
            chunk = bytes(self.uc.mem_read(a + len(out), 64))
            z = chunk.find(b"\0")
            if z >= 0:
                return bytes(out + chunk[:z])
            out += chunk
        return bytes(out)

    def _trampoline(self, uc, addr, size, data):
        i = (addr - TRAMPOLINE_CODE) // 16
        r0, r1, r2 = (self.reg(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
        ret = 0
        if i == 3:  # memcmp
            a, b = bytes(uc.mem_read(r0, r2)), bytes(uc.mem_read(r1, r2))
            ret = 0
            for x, y in zip(a, b):
                if x != y:
                    ret = (x - y) & 0xFFFFFFFF
                    break
        elif i in (4, 5):  # memcpy, memmove
            if r2:
                uc.mem_write(r0, bytes(uc.mem_read(r1, r2)))
            ret = r0
        elif i == 6:  # memset
            if r2:
                uc.mem_write(r0, bytes([r1 & 0xFF]) * r2)
            ret = r0
        elif i == 7:  # strchr
            s = self._cstr(r0) + b"\0"
            k = s.find(bytes([r1 & 0xFF]))
            ret = r0 + k if k >= 0 else 0
        elif i == 8:  # strcmp
            a, b = self._cstr(r0) + b"\0", self._cstr(r1) + b"\0"
            ret = 0
            for x, y in zip(a, b):
                if x != y:
                    ret = (x - y) & 0xFFFFFFFF
                    break
        elif i == 9:  # strlcat(dst, src, size)
            d, sr = self._cstr(r0), self._cstr(r1)
            room = r2 - len(d) - 1
            if room > 0:
                uc.mem_write(r0 + len(d), sr[:room] + b"\0")
            ret = len(d) + len(sr)
        elif i == 10:  # strlcpy
            sr = self._cstr(r1)
            if r2:
                uc.mem_write(r0, sr[:r2 - 1] + b"\0")
            ret = len(sr)
        elif i == 11:  # strlen
            ret = len(self._cstr(r0))
        elif i == 12:  # strncmp
            a, b = self._cstr(r0, r2) + b"\0", self._cstr(r1, r2) + b"\0"
            ret = 0
            for x, y in list(zip(a, b))[:r2]:
                if x != y:
                    ret = (x - y) & 0xFFFFFFFF
                    break
                if x == 0:
                    break
        else:
            self.faults.append(f"trampoline {i} (flash or suspend) called")
        uc.reg_write(UC_ARM_REG_R0, ret & 0xFFFFFFFF)
        uc.reg_write(UC_ARM_REG_PC, self.reg(UC_ARM_REG_LR))

    def _invalid(self, uc, access, addr, size, value, data):
        pc = uc.reg_read(UC_ARM_REG_PC)
        self.faults.append(f"invalid memory access at {addr:#x} (pc {pc:#x}, {self.symbol_at(pc)})")
        return False

    def symbol_at(self, pc):
        best, name = 0, "?"
        for n, v in self.symbols.items():
            v &= ~1
            if best < v <= pc:
                best, name = v, n
        return name

    def _intr(self, uc, intno, data):
        if intno != 2:
            pc = self.reg(UC_ARM_REG_PC)
            self.faults.append(f"exception {intno} at pc {pc:#x} ({self.symbol_at(pc)})")
            uc.emu_stop()
            return
        pc = self.reg(UC_ARM_REG_PC)
        op, = struct.unpack("<H", uc.mem_read(pc - 2, 2))
        n = op & 0xFF
        self.svc_counts[n] = self.svc_counts.get(n, 0) + 1
        r0, r1, r2, r3 = (self.reg(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))
        ret = ret1 = None
        if n in (18, 19, 20, 15 + 100):  # pull, push, push uniform
            x, y, w, h = r0 & 0xFFFF, r0 >> 16, r1 & 0xFFFF, r1 >> 16
            w, h = max(0, min(w, 320 - x)), max(0, min(h, 240 - y))
            if n == 20:
                c = struct.pack("<H", r2 & 0xFFFF) * w
                for j in range(h):
                    o = ((y + j) * 320 + x) * 2
                    self.screen[o:o + 2 * w] = c
            elif n == 19:
                src = bytes(uc.mem_read(r2, w * h * 2)) if w and h else b""
                for j in range(h):
                    o = ((y + j) * 320 + x) * 2
                    self.screen[o:o + 2 * w] = src[j * w * 2:(j + 1) * w * 2]
            else:
                out = bytearray()
                for j in range(h):
                    o = ((y + j) * 320 + x) * 2
                    out += self.screen[o:o + 2 * w]
                if out:
                    uc.mem_write(r2, bytes(out))
            self.now_ms += w * h / 20000.0
        elif n == 17:  # post push multicolor
            pass
        elif n == 21:  # wait for vblank
            self.now_ms += 4
            ret = 1
        elif n in (33, 34):  # keyboard pop state / scan
            s = self.keyboard_state()
            ret, ret1 = s & 0xFFFFFFFF, s >> 32
            self.now_ms += 0.05
        elif n == 48:
            ms = int(self.now_ms)
            ret, ret1 = ms & 0xFFFFFFFF, ms >> 32
        elif n == 49:
            self.now_ms += r0
        elif n == 50:
            self.now_ms += r0 / 1000
        elif n == 45:
            self.rng ^= (self.rng << 13) & 0xFFFFFFFF
            self.rng ^= self.rng >> 17
            self.rng ^= (self.rng << 5) & 0xFFFFFFFF
            ret = self.rng
        elif n == 23:  # get event: key press edges, else wait for the timeout
            s = self.keyboard_state()
            pressed = s & ~self.prev_state
            self.prev_state = s
            ret = EVENT_NONE
            for k in range(64):
                if pressed >> k & 1:
                    ret = k
                    break
            if ret == EVENT_NONE:
                timeout = struct.unpack("<i", uc.mem_read(r0, 4))[0] if r0 else 0
                wait = max(1, min(timeout, 20))
                self.now_ms += wait
                if r0:
                    uc.mem_write(r0, struct.pack("<i", max(0, timeout - wait)))
        elif n == 22:  # copy text
            ret = 0
        elif n == 24:  # event is defined
            ret = 1 if r0 < 216 else 0
        elif n == 26:
            self.shift_alpha = r0 & 0xFF
        elif n == 28:
            ret = self.shift_alpha
        elif n == 55:
            ret = 0
        elif n == 1:
            ret = 200
        elif n == 3:
            ret = 0
        elif n == 4:
            ret = 3  # Charge::FULL
        elif n == 5:
            ret = 0x40800000  # 4.0 V as float bits
        elif n == 52:  # USB plugged
            ret = 0
        elif n == 53:  # USB should interrupt
            ret = 0
        elif n == 8:  # has checkpoint
            ret = 0
        elif n == 11:  # set checkpoint: Status::Set
            ret = 2
        elif n == 12:
            ret = 2
        elif n == 10:
            self.locked += 1
        elif n == 13:
            self.locked -= 1
        elif n == 15:  # crc32 bytes
            ret = epsilon_crc32(bytes(uc.mem_read(r0, r1))) if r1 else 0
        elif n == 16:  # crc32 words
            ret = epsilon_crc32(bytes(uc.mem_read(r0, 4 * r1))) if r1 else 0
        elif n == 0:  # authentication clearance level: third party
            ret = 2
        elif n == 56:  # compilation flags
            ret = 0
        elif n == 39:  # pcb version
            ret = 1
        elif n == 59:  # last reset type
            ret = 0
        elif n == 46:  # reset core
            self.resets += 1
            self.faults.append(f"the userland asked for a reset (pc {pc:#x}, {self.symbol_at(self.reg(UC_ARM_REG_LR))})")
            uc.emu_stop()
        elif n in (2, 6, 7, 9, 14, 27, 35, 36, 37, 38, 42, 43, 44, 51, 54, 58):
            pass
        else:
            self.unhandled[n] = self.unhandled.get(n, 0) + 1
            ret = 0
        if self.trace_svc:
            print(f"svc {n} r0={r0:#x} -> {ret}", file=sys.stderr)
        if ret is not None:
            uc.reg_write(UC_ARM_REG_R0, ret & 0xFFFFFFFF)
        if ret1 is not None:
            uc.reg_write(UC_ARM_REG_R1, ret1 & 0xFFFFFFFF)

    # ---------------------------------------------------------------- running
    def run(self, until_ms, shots=(), out=None):
        shots = sorted(shots)
        chunk = CPU_HZ // 4000
        while not self.faults and self.now_ms < until_ms:
            try:
                self.uc.emu_start(self.pc, 0xFFFFFFFF, count=chunk)
            except UcError as err:
                pc = self.reg(UC_ARM_REG_PC)
                self.faults.append(f"CPU fault {err} at pc {pc:#x} ({self.symbol_at(pc)})")
                break
            self.pc = self.reg(UC_ARM_REG_PC) | 1
            self.now_ms += 0.25
            while shots and shots[0] <= self.now_ms:
                t = shots.pop(0)
                if out:
                    self.save_png(os.path.join(out, f"shot_{int(t):06d}.png"))


def parse_keys(spec):
    out = []
    for item in filter(None, (spec or "").split(",")):
        rng, k = item.split(":")
        a, _, b = rng.partition("-")
        out.append((float(a), float(b or float(a) + 100), KEYS[k] if k in KEYS else int(k)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--version", default="26.4.0")
    ap.add_argument("--ms", type=float, default=5000)
    ap.add_argument("--keys", default="")
    ap.add_argument("--shots", default="")
    ap.add_argument("--out", default=".")
    ap.add_argument("--trace-svc", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    u = Userland(a.elf, a.version, parse_keys(a.keys), a.trace_svc)
    shots = [float(s) for s in a.shots.split(",") if s]
    u.run(a.ms, shots, a.out)
    u.save_png(os.path.join(a.out, "final.png"))
    print(f"virtual time {u.now_ms:.0f} ms; svc counts {dict(sorted(u.svc_counts.items()))}")
    if u.unhandled:
        print(f"unhandled svc (answered 0): {u.unhandled}")
    for f in u.faults:
        print("FAULT:", f)
    return 1 if u.faults else 0


if __name__ == "__main__":
    sys.exit(main())
