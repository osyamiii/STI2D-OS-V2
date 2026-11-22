#!/usr/bin/env python3
"""Pre-install .nwa apps (games, AI apps) into the STI2D-OS userland's
external apps area, as NumWorks' installer (nwlink) does from the website.

Each relocatable .nwa is linked at its final flash address inside the slot's
external apps area (read from the userland ELF), with the EADK runtime of the
Epsilon tree, and the results are concatenated (each app aligned on the
external apps sector unit) into one image, flashed at the start of the area.
The STI2D-OS shell then lists them in Jeux / Mes apps / Matrices IA.

Usage:
  link_apps.py --model n0120 <userland.elf> <out.bin> app1.nwa [app2.nwa ...]
  link_apps.py --model n0120 --dry-run <userland.elf> app1.nwa ...   (sizes only)

Adapted from the PSP-OS installer's link_games.py (same user, same method).
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "epsilon-src"
EPSILON = ROOT / "epsilon"
EADK_SRC = EPSILON / "eadk" / "src"
SECTOR = 0x10000  # Config::ExternalAppsSectorUnit (all three models)

# Config::TrampolineOrigin (the bootloader's table in internal flash) and the
# CPU of each model (shared/ion/src/device/include/<model>/config/board.h,
# haussmann/src/configs/<model>.mak)
MODELS = {
    "n0110": {"trampoline": 0x0020E000,
              "cpu": ["-mcpu=cortex-m7", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"]},
    "n0115": {"trampoline": 0x0020E000,
              "cpu": ["-mcpu=cortex-m7", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv5-sp-d16"]},
    "n0120": {"trampoline": 0x0807FC00,
              "cpu": ["-mcpu=cortex-m7", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv5-d16"]},
}


def run(cmd):
    subprocess.run(cmd, check=True)


def symbols(elf):
    out = subprocess.run(["arm-none-eabi-nm", elf], check=True,
                         capture_output=True, text=True).stdout
    table = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            table[parts[2]] = int(parts[0], 16)
    return table


def build_eadk(tmp, cpu):
    objs = []
    inc = ["-I", str(EPSILON / "eadk" / "include"), "-I", str(ROOT / "shared"),
           "-I", str(ROOT / "shared" / "ion" / "src" / "device")]
    for src, extra in (("platform.c", []),
                       ("svc.cpp", ["-std=c++20", "-fno-exceptions", "-fno-rtti"]),
                       ("trampoline.cpp", ["-std=c++20", "-fno-exceptions", "-fno-rtti"])):
        obj = os.path.join(tmp, src + ".o")
        run(["arm-none-eabi-gcc", *cpu, "-Os", "-DPLATFORM_DEVICE=1", "-ffunction-sections",
             "-fdata-sections", *extra, *inc, "-c", str(EADK_SRC / src), "-o", obj])
        objs.append(obj)
    return objs


def linker_script(flash_start, flash_length, ram_start, ram_length, trampoline):
    ld = (EADK_SRC / "linker.ld").read_text()
    ld = (ld.replace("{{ flashStart }}", hex(flash_start))
            .replace("{{ flashLength }}", hex(flash_length))
            .replace("{{ ramStart }}", hex(ram_start))
            .replace("{{ ramLength }}", hex(ram_length))
            .replace("{{ trampolineStart }}", hex(trampoline)))
    # The app size stored in the header must cover the .data initial values
    # stored in flash after .rodata, otherwise the next app could overlap them.
    ld = ld.replace("    _eadk_app_end = .;\n", "")
    ld += ("\n_eadk_app_end = LOADADDR(.data) + SIZEOF(.data);\n"
           "_eadk_external_data_start = _eadk_app_end;\n"
           "_eadk_external_data_size = 0;\n")
    return ld


def area(userland_elf):
    sym = symbols(userland_elf)
    return (sym["_external_apps_flash_start"], sym["_external_apps_flash_end"],
            (sym["_external_apps_RAM_start"] + 7) & ~7, sym["_external_apps_RAM_end"])


def link(model, userland_elf, apps, out_bin=None, log=print):
    cfg = MODELS[model]
    area_start, area_end, ram_start, ram_end = area(userland_elf)
    image = bytearray()
    placed = []
    with tempfile.TemporaryDirectory() as tmp:
        eadk = build_eadk(tmp, cfg["cpu"])
        for app in apps:
            app = Path(app)
            address = area_start + len(image)
            ld_path = os.path.join(tmp, "app.ld")
            Path(ld_path).write_text(linker_script(address, area_end - address, ram_start,
                                                   ram_end - ram_start, cfg["trampoline"]))
            elf = os.path.join(tmp, "app.elf")
            binary = os.path.join(tmp, "app.bin")
            run(["arm-none-eabi-gcc", *cfg["cpu"], "-nostartfiles", "-nostdlib",
                 "-T", ld_path, "-Wl,--gc-sections", "-Wl,-e,_start",
                 str(app), *eadk, "-lgcc", "-o", elf])
            run(["arm-none-eabi-objcopy", "-O", "binary", elf, binary])
            data = Path(binary).read_bytes()
            if int.from_bytes(data[0:4], "little") != 0xDEC0BEBA:
                raise ValueError(f"{app.name}: not an EADK app")
            size_field = int.from_bytes(data[24:28], "little")
            if size_field < len(data):
                raise ValueError(f"{app.name}: inconsistent app size")
            padded = (len(data) + SECTOR - 1) // SECTOR * SECTOR
            if area_start + len(image) + padded + SECTOR > area_end:
                raise ValueError(f"{app.name}: the apps do not fit in the slot's app area "
                                 f"({(area_end - area_start) // 1024} KB)")
            image += data + b"\xff" * (padded - len(data))
            placed.append((app.name, address, len(data)))
            log(f"{app.name:32s} @ {address:#x}  {len(data):8d} bytes")
    # One erased sector after the last app ends the app list
    image += b"\xff" * SECTOR
    if out_bin:
        Path(out_bin).write_bytes(image)
    log(f"Image: {len(image)} bytes for {area_start:#x}..{area_start + len(image):#x} "
        f"(area {area_end - area_start} bytes)")
    return image, area_start, placed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True, choices=sorted(MODELS))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("userland_elf")
    ap.add_argument("rest", nargs="+")
    a = ap.parse_args()
    if a.dry_run:
        link(a.model, a.userland_elf, a.rest)
    else:
        link(a.model, a.userland_elf, a.rest[1:], a.rest[0])


if __name__ == "__main__":
    sys.exit(main())
