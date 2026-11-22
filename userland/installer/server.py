#!/usr/bin/env python3
"""STI2D-OS userland installer: a small local web page around sti2dos_install.py.

Listens on 127.0.0.1 only. Shows the calculator (model, official version,
free slot), the apps of the project's library (library/ai, library/games) and
the ones added here, and installs STI2D-OS with the chosen apps in the
inactive slot. Same method as the PSP-OS installer.

  python3 userland/installer/server.py [--port 8766] [--no-browser]
"""

import argparse
import json
import os
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote

import sti2dos_install as inst

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent.parent
LIBRARY = [("ai", PROJECT / "library" / "ai"), ("game", PROJECT / "library" / "games"),
           ("mine", HERE / "apps")]
PAGE = HERE / "index.html"
MAX_UPLOAD = 6 * 1024 * 1024


def app_name(path):
    """The name the app shows (its .rodata.eadk_app_name), or None if not an EADK app."""
    import subprocess
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "n.bin")
        p = subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "--only-section=.rodata.eadk_app_name",
                            str(path), out], capture_output=True)
        if p.returncode or not os.path.exists(out):
            return None
        raw = Path(out).read_bytes().split(b"\0")[0]
    return raw.decode("utf-8", "replace") or None


_names = {}


def library():
    items = []
    for kind, folder in LIBRARY:
        if not folder.exists():
            continue
        for p in sorted(folder.glob("*.nwa"), key=lambda p: p.name.lower()):
            key = (str(p), p.stat().st_mtime)
            if key not in _names:
                _names[key] = app_name(p)
            items.append({"id": f"{kind}/{p.name}", "kind": kind, "file": p.name,
                          "name": _names[key] or p.stem, "size": p.stat().st_size})
    return items


def resolve(app_id):
    kind, _, name = app_id.partition("/")
    name = os.path.basename(unquote(name))
    for k, folder in LIBRARY:
        if k == kind and name.endswith(".nwa"):
            p = folder / name
            if p.exists():
                return p
    return None


class Job:
    def __init__(self):
        self.lock = threading.Lock()
        self.running, self.steps, self.log, self.result = False, [], [], None

    def snapshot(self):
        with self.lock:
            return {"running": self.running, "steps": list(self.steps),
                    "log": "\n".join(self.log[-300:]), "result": self.result}

    def step(self, label):
        with self.lock:
            if self.steps and self.steps[-1]["state"] == "running":
                self.steps[-1]["state"] = "done"
            self.steps.append({"label": label, "state": "running"})

    def write(self, text):
        with self.lock:
            self.log += [l.rstrip() for l in str(text).splitlines() if l.strip()]


JOB = Job()


def run_install(paths):
    try:
        inst.install(paths, log=JOB.write, step=JOB.step)
        with JOB.lock:
            if JOB.steps:
                JOB.steps[-1]["state"] = "done"
            JOB.result = "ok"
    except Exception as e:  # surfaced to the page
        with JOB.lock:
            if JOB.steps:
                JOB.steps[-1]["state"] = "error"
            JOB.log.append("ERREUR : " + str(e))
            JOB.result = "error"
    finally:
        with JOB.lock:
            JOB.running = False


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send_json(self, data, code=200):
        body = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def same_origin(self):
        origin = self.headers.get("Origin")
        return origin is None or origin == f"http://{self.headers.get('Host', '')}"

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            body = PAGE.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/api/status":
            busy = JOB.snapshot()["running"]
            calc = {"connected": None} if busy else inst.status()
            capacity = None
            if calc.get("connected") and calc.get("inactiveSlot"):
                files = inst.firmware_files(calc["model"], calc["inactiveSlot"])
                if files:
                    start, end, _, _ = inst.link_apps.area(str(files[1]))
                    capacity = end - start - inst.link_apps.SECTOR
            self.send_json({"calculator": calc, "apps": library(), "capacity": capacity,
                            "sector": inst.link_apps.SECTOR})
        elif self.path == "/api/job":
            self.send_json(JOB.snapshot())
        else:
            self.send_json({"error": "not found"}, 404)

    def do_POST(self):
        if not self.same_origin():
            return self.send_json({"error": "forbidden"}, 403)
        length = int(self.headers.get("Content-Length", "0"))
        if self.path == "/api/install":
            ids = json.loads(self.rfile.read(length) or b"{}").get("apps", [])
            paths = [resolve(i) for i in ids]
            if any(p is None for p in paths):
                return self.send_json({"error": "App introuvable."}, 400)
            with JOB.lock:
                if JOB.running:
                    return self.send_json({"error": "Installation déjà en cours."}, 409)
                JOB.running, JOB.steps, JOB.log, JOB.result = True, [], [], None
            threading.Thread(target=run_install, args=([str(p) for p in paths],), daemon=True).start()
            return self.send_json({"started": True})
        if self.path == "/api/apps":
            name = os.path.basename(unquote(self.headers.get("X-Filename", "")))
            if not name.lower().endswith(".nwa") or name.startswith("."):
                return self.send_json({"error": "Seuls les fichiers .nwa sont acceptés."}, 400)
            if length <= 0 or length > MAX_UPLOAD:
                return self.send_json({"error": "Fichier vide ou trop gros (6 Mo max)."}, 400)
            data = self.rfile.read(length)
            if data[:4] != b"\x7fELF" or data[4] != 1 or int.from_bytes(data[18:20], "little") != 40:
                return self.send_json({"error": "Ce n'est pas un ELF ARM 32 bits (.nwa)."}, 400)
            (HERE / "apps").mkdir(exist_ok=True)
            target = HERE / "apps" / name
            target.write_bytes(data)
            if app_name(target) is None:
                target.unlink()
                return self.send_json({"error": "Pas une app NumWorks (aucun nom d'app EADK)."}, 400)
            return self.send_json({"ok": True})
        self.send_json({"error": "not found"}, 404)

    def do_DELETE(self):
        if not self.same_origin():
            return self.send_json({"error": "forbidden"}, 403)
        if self.path.startswith("/api/apps/mine/"):
            p = resolve("mine/" + self.path[len("/api/apps/mine/"):])
            if p is None:
                return self.send_json({"error": "App introuvable."}, 404)
            p.unlink()
            return self.send_json({"ok": True})
        self.send_json({"error": "not found"}, 404)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8766)
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    url = f"http://127.0.0.1:{a.port}/"
    print(f"Installateur STI2D-OS : {url}  (Ctrl+C pour quitter)")
    if not a.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
