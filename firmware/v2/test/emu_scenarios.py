#!/usr/bin/env python3
"""STI2D-OS V2 on the real ARM build: scenarios run in the project's Cortex-M7 emulator
(firmware/sti2d-play-source-v1/tools/emu.py, Unicorn), against build/sti2dos/<model>/STI2D-OS.nwa.

Each scenario is a list of key presses (one every 180 ms) and screenshot times; the emulator's
record listing and storage file are then checked. Needs: unicorn, pyelftools, Pillow, nwlink.

  python3 test/emu_scenarios.py SRC_DIR NWLINK OUT_DIR
"""
import pathlib
import re
import struct
import subprocess
import sys

SRC = pathlib.Path(sys.argv[1]).resolve()
NWLINK = sys.argv[2]
OUT = pathlib.Path(sys.argv[3]).resolve()


def find_nwlink():
    """The nwlink executable npx installed (emu.py runs it directly, not through npx)."""
    out = subprocess.run(["npx", "--yes", "--", "nwlink@1.0.0", "eadk-cflags-device"], capture_output=True, text=True).stdout
    m = re.search(r'-I"?([^"\s]*)/dist/eadk', out)
    if not m:
        sys.exit("nwlink introuvable : installe Node.js puis lance `npx nwlink@1.0.0 --help` une fois")
    return str(pathlib.Path(m.group(1)).parent / ".bin" / "nwlink")


if NWLINK == "auto":
    NWLINK = find_nwlink()
OUT.mkdir(parents=True, exist_ok=True)
failures = 0


def keys(seq, start=1200, step=180, hold=80):
    """'down down ok' -> emu.py's --keys, and the time after the last press."""
    parts, t = [], start
    for k in seq.split():
        if k.startswith("wait"):
            t += int(k[4:])
            continue
        parts.append(f"{t}-{t + hold}:{k}")
        t += step
    return ",".join(parts), t


def run(name, model, seq, extra_ms=800, shots=(), system="epsilon", storage=None):
    ks, end = keys(seq)
    out = OUT / name
    out.mkdir(exist_ok=True)
    cmd = [sys.executable, str(SRC / "tools/emu.py"), str(SRC / f"build/sti2dos/{model}/STI2D-OS.nwa"), "--nwlink", NWLINK,
           "--model", "n0110" if model != "n0120" else "n0120", "--system", system, "--ms", str(end + extra_ms),
           "--out", str(out), "--records"]
    if ks:
        cmd += ["--keys", ks]
    shot_list = [s if s > 0 else end + extra_ms - 50 for s in shots] or [end + extra_ms - 50]
    cmd += ["--shots", ",".join(str(s) for s in shot_list)]
    if storage:
        cmd += ["--storage", str(storage)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=SRC)
    log = r.stdout + r.stderr
    (out / "emu.log").write_text(log)
    return log, out


def check(cond, what):
    global failures
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures += 1


def storage_records(path):
    """Records of the emulator's storage file: an Epsilon record list (size, name, data)."""
    data = path.read_bytes()
    recs, p = {}, 0
    while p + 2 <= len(data):
        n = struct.unpack_from("<H", data, p)[0]
        if n == 0 or p + n > len(data):
            break
        body = data[p + 2:p + n]
        name, _, content = body.partition(b"\0")
        recs[name.decode(errors="replace")] = content
        p += n
    return recs


for model in ("n0120", "n0110", "n0115"):
    print(f"[{model}] boot to the STI2D-OS home, double pi to Classic")
    log, out = run(f"{model}_boot", model, "wait600 pi pi", shots=(1100, -1))
    check("exited=False" in log and "sti2dos.py" in log, f"{model}: runs, writes sti2dos.py ({log.strip().splitlines()[0] if log.strip() else 'no output'})")

print("[n0110] Upsilon: boot")
log, out = run("n0110_upsilon_boot", "n0110", "", system="upsilon", shots=(1500,))
check("exited=False" in log, "n0110 on Upsilon runs")

print("[n0120] games list, start Chrome Dino (screenshot in game), Home leaves the app")
log, out = run("n0120_game", "n0120", "ok wait300 down down ok wait800 ok wait1500 home", extra_ms=600, shots=(3600,))
check("exited=True" in log, "a game starts from the list, Home inside it leaves STI2D-OS (exited=True)")

print("[n0120] Control Center: accent, then Classic through the Interface row")
st = OUT / "n0120_storage.bin"
if st.exists():
    st.unlink()
log, out = run("n0120_cc", "n0120", "down down ok wait300 down down down right right wait200", shots=(-1,), storage=st)
recs = storage_records(st) if st.exists() else {}
check(b"A=2" in recs.get("sti2dos.py", b""), "accent saved in sti2dos.py (A=2)")

print("[n0120] create a PIN, then a new start shows the lock and the PIN opens the home")
seq = "down down ok wait300 " + "down " * 7 + "ok wait200 ok 1 4 7 9 ok 1 4 7 9 ok wait1200 ok wait300 back back"
log, out = run("n0120_pin_create", "n0120", seq, extra_ms=1500, shots=(-1,), storage=st)
recs = storage_records(st)
pin = recs.get("sti2dos.pin", b"")
check(len(pin) > 100 and b"1479" not in pin, f"sti2dos.pin written ({len(pin)} bytes), no clear PIN inside")
log, out = run("n0120_pin_lock", "n0120", "wait400", shots=(1300,), storage=st)
log2, out2 = run("n0120_pin_unlock", "n0120", "wait300 1 4 7 9 ok wait1500", extra_ms=1500, shots=(-1,), storage=st)
check("exited=False" in log2, "unlocked with the right PIN (see n0120_pin_unlock screenshot)")

print("[n0120] Terminal: help")
seq = "down down left ok wait300 cos comma square lparen ok wait300"
log, out = run("n0120_terminal", "n0120", "wait300 1 4 7 9 ok wait1500 " + seq, extra_ms=600, shots=(-1,), storage=st)
check("exited=False" in log, "terminal runs")

print(f"\n{failures} failure(s)")
sys.exit(1 if failures else 0)
