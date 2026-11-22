#!/usr/bin/env python3
"""STI2D-OS userland installer (command line; server.py gives it a web page).

Writes the STI2D-OS userland to the calculator's INACTIVE slot over DFU and
pre-installs the chosen .nwa apps in that slot's external apps area. The
official software in the active slot is never touched: a press on Reset (on
the back) always brings the calculator back to it.

  python3 sti2dos_install.py --status
  python3 sti2dos_install.py --apps ../../library/ai/Matrices-v4.nwa ../../library/games/Snake.nwa
  python3 sti2dos_install.py --no-apps          (the system only)

Requirements: dfu-util, arm-none-eabi-gcc (for the apps), Python 3 with pyusb
(Epsilon's tools/device/dfu.py). The calculator must show "La calculatrice
est connectée" (plugged in, official software running).
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
USERLAND = HERE.parent
EPSILON = USERLAND / "epsilon-src" / "epsilon"
DFU_PY = EPSILON / "tools" / "device" / "dfu.py"
FIRMWARE = USERLAND / "firmware"  # <model>/sti2d-os.<slot>.dfu|.elf (userland/build.sh)
sys.path.insert(0, str(USERLAND / "tools"))
import link_apps  # noqa: E402

PYTHON = sys.executable
DFU_UTIL = shutil.which("dfu-util") or "/opt/homebrew/bin/dfu-util"
SLOT = {"A": 0x90000000, "B": 0x90400000}
KERNEL_HEADER = 8          # Config::SignedPayloadLength: the kernel header follows
USERLAND_OFFSET = 0x10000  # after the 64 KiB kernel (EMBED_EXTRA_DATA=0 builds)


def run(cmd, timeout=600, cwd=EPSILON):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def dfu_query(flag):
    code, out = run([PYTHON, str(DFU_PY), flag], timeout=30)
    lines = [l.strip() for l in out.strip().splitlines() if l.strip()]
    if code != 0 or not lines or "Error" in out:
        return None
    return lines[-1]


def official_version(active_slot):
    """The software version in the kernel header of the running (official) slot."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "kh.bin")
        address = SLOT[active_slot] + KERNEL_HEADER
        code, log = run([DFU_UTIL, "-a", "0", "-s", f"{hex(address)}:24", "-U", out], timeout=60)
        if code != 0 or not os.path.exists(out):
            return None
        data = Path(out).read_bytes()
    if len(data) < 24 or int.from_bytes(data[0:4], "little") != 0xDEC00DF0:
        return None
    return data[4:12].split(b"\0")[0].decode("ascii", "replace")


def firmware_version(elf):
    """The version the userland was built for (its userland header)."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "uh.bin")
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "--only-section=.userland_header",
                        str(elf), out], capture_output=True)
        data = Path(out).read_bytes() if os.path.exists(out) else b""
    return data[4:12].split(b"\0")[0].decode("ascii", "replace") if len(data) >= 12 else None


def status():
    model = dfu_query("--model")
    if model is None:
        return {"connected": False}
    inactive = dfu_query("--inactive_slot")
    active = {"A": "B", "B": "A"}.get(inactive)
    info = {"connected": True, "model": model, "inactiveSlot": inactive, "activeSlot": active}
    if active:
        info["officialVersion"] = official_version(active)
    files = firmware_files(model, inactive) if inactive else None
    if files:
        info["firmwareVersion"] = firmware_version(files[1])
    return info


def firmware_files(model, slot):
    dfu = FIRMWARE / model / f"sti2d-os.{slot}.dfu"
    elf = FIRMWARE / model / f"sti2d-os.{slot}.elf"
    return (dfu, elf) if dfu.exists() and elf.exists() else None


def install(apps, log=print, step=print, allow_version_mismatch=False):
    step("Recherche de la calculatrice")
    st = status()
    if not st["connected"]:
        raise RuntimeError("Calculatrice introuvable. Branche-la en USB sur le logiciel officiel "
                           "(écran « La calculatrice est connectée »), ferme my.numworks.com, réessaie.")
    model, slot = st["model"], st["inactiveSlot"]
    log(f"Modèle {model}, slot officiel {st['activeSlot']} ({st.get('officialVersion') or '?'}), "
        f"slot libre {slot}")
    if model not in link_apps.MODELS:
        raise RuntimeError(f"Modèle {model} non pris en charge.")
    if slot not in SLOT:
        raise RuntimeError("Slot inactif inconnu : installation annulée.")
    files = firmware_files(model, slot)
    if not files:
        raise RuntimeError(f"Pas de STI2D-OS compilé pour {model} slot {slot} "
                           f"(userland/firmware/{model}/). Lance userland/build.sh {model}.")
    dfu, elf = files
    official, ours = st.get("officialVersion"), st.get("firmwareVersion")
    if official and ours and official != ours and not allow_version_mismatch:
        raise RuntimeError(f"La calculatrice a Epsilon {official}, ce STI2D-OS est fait pour {ours} : "
                           f"le noyau officiel le refuserait. Recompile avec "
                           f"APP_VERSION={official} userland/build.sh {model}.")
    if apps:
        step(f"Préparation de {len(apps)} app(s)")
        with tempfile.TemporaryDirectory() as tmp:
            image_path = os.path.join(tmp, "apps.bin")
            image, start, placed = link_apps.link(model, str(elf), apps, image_path, log=log)
            step("Copie des apps sur la calculatrice")
            code, out = run([DFU_UTIL, "-a", "0", "-s", hex(start), "-D", image_path])
            if code != 0:
                log(out)
                raise RuntimeError("Échec de l'écriture des apps.")
            step("Vérification des apps")
            back = os.path.join(tmp, "readback.bin")
            code, out = run([DFU_UTIL, "-a", "0", "-s", f"{hex(start)}:{len(image)}", "-U", back])
            if code != 0 or Path(back).read_bytes() != bytes(image):
                log(out)
                raise RuntimeError("Les apps relues ne correspondent pas à ce qui a été écrit.")
            log("Apps vérifiées : identiques.")
    step(f"Installation de STI2D-OS (slot {slot})")
    if dfu_query("--inactive_slot") != slot:
        raise RuntimeError("Le slot inactif a changé, installation annulée.")
    address = SLOT[slot] + USERLAND_OFFSET
    code, out = run([PYTHON, str(DFU_PY), "-s", f"{hex(address)}:leave", "-D", str(dfu)])
    log("\n".join(l for l in out.splitlines() if "%" not in l))
    if code != 0:
        raise RuntimeError("Échec de l'écriture de STI2D-OS.")
    step("STI2D-OS démarre sur la calculatrice")
    return st


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--status", action="store_true")
    ap.add_argument("--apps", nargs="*", default=None)
    ap.add_argument("--no-apps", action="store_true")
    ap.add_argument("--force-version", action="store_true",
                    help="flash even if the official version differs (it will not start)")
    a = ap.parse_args()
    if a.status:
        print(status())
        return 0
    apps = [] if a.no_apps else (a.apps or [])
    try:
        install(apps, allow_version_mismatch=a.force_version)
    except RuntimeError as e:
        print("ERREUR:", e)
        return 1
    print("Terminé.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
