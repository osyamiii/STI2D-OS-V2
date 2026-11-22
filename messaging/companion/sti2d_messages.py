#!/usr/bin/env python3
"""STI2D Messages desktop companion: account, PC chat and serial bridge."""
from __future__ import annotations
import argparse, asyncio, getpass, json, os, secrets, sys, time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

try:
    import websockets
except ImportError:
    websockets = None

APP_DIR = Path.home() / ".config" / "sti2d-messages"
TOKEN_FILE = APP_DIR / "account.json"
STATE_FILE = APP_DIR / "bridge-state.json"
MAX_LINE = 4096

class ApiError(Exception): pass

def load_account():
    try:
        data = json.loads(TOKEN_FILE.read_text("utf-8"))
        if isinstance(data.get("token"), str) and isinstance(data.get("username"), str):
            return data
    except (OSError, ValueError, AttributeError):
        pass
    return None

def save_account(username, token):
    APP_DIR.mkdir(parents=True, exist_ok=True)
    TOKEN_FILE.write_text(json.dumps({"username": username, "token": token}, ensure_ascii=False), "utf-8")
    try: TOKEN_FILE.chmod(0o600)
    except OSError: pass

def api(base, path, token=None, payload=None):
    body = None if payload is None else json.dumps(payload, ensure_ascii=False).encode("utf-8")
    headers = {"Accept": "application/json"}
    if body is not None: headers["Content-Type"] = "application/json"
    if token: headers["Authorization"] = "Bearer " + token
    req = Request(base.rstrip("/") + path, data=body, headers=headers, method="POST" if body is not None else "GET")
    try:
        with urlopen(req, timeout=15) as response:
            return json.loads(response.read().decode("utf-8"))
    except HTTPError as e:
        try: detail = json.loads(e.read().decode("utf-8")).get("detail", str(e))
        except Exception: detail = str(e)
        raise ApiError(str(detail)) from e
    except URLError as e:
        raise ApiError("Serveur inaccessible : " + str(e.reason)) from e

def account_or_exit():
    account = load_account()
    if not account: raise ApiError("Aucun compte configuré. Lance d'abord register ou login.")
    return account

def serial_packet(packet):
    """Strict line framing shared with the firmware protocol: UTF-8 JSON + LF."""
    raw = json.dumps(packet, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(raw) > MAX_LINE - 1: raise ValueError("Paquet trop long")
    return raw + b"\n"

def parse_device_line(raw):
    if len(raw) > MAX_LINE: raise ValueError("Ligne USB trop longue")
    packet = json.loads(raw.decode("utf-8"))
    if not isinstance(packet, dict) or packet.get("v") != 1:
        raise ValueError("Version de protocole invalide")
    if packet.get("type") != "send": raise ValueError("Type de paquet inconnu")
    recipient, text = packet.get("to"), packet.get("text")
    if not isinstance(recipient, str) or not 3 <= len(recipient) <= 24:
        raise ValueError("Destinataire invalide")
    if not isinstance(text, str) or not text.strip() or len(text) > 1000:
        raise ValueError("Message vide ou trop long")
    client_id = str(packet.get("id") or secrets.token_hex(12))[:64]
    if len(client_id) < 8 or any(ch not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for ch in client_id):
        raise ValueError("Identifiant de message invalide")
    return recipient, text.strip(), client_id

def load_state():
    try: return json.loads(STATE_FILE.read_text("utf-8"))
    except (OSError, ValueError): return {"last_id": 0}

def save_state(state):
    APP_DIR.mkdir(parents=True, exist_ok=True)
    STATE_FILE.write_text(json.dumps(state), "utf-8")

def ws_url(base):
    if base.startswith("https://"): return "wss://" + base[8:].rstrip("/") + "/v1/live"
    if base.startswith("http://"): return "ws://" + base[7:].rstrip("/") + "/v1/live"
    raise ApiError("L'URL du serveur doit commencer par http:// ou https://")

async def listen(base, account, output=print):
    if websockets is None: raise ApiError("Dépendance websockets absente. Installe requirements-messages.txt")
    async with websockets.connect(ws_url(base), additional_headers={"Authorization": "Bearer " + account["token"]}, ping_interval=20, max_size=8192) as ws:
        ready = json.loads(await ws.recv())
        output("Connecté en tant que @" + ready.get("username", account["username"]))
        # Open the live channel first, then load offline messages: this closes
        # the gap where a message could arrive between inbox polling and WS connect.
        backlog = await asyncio.to_thread(api, base, "/v1/inbox?after_id=0&limit=100", account["token"])
        last_id = 0
        for m in backlog.get("messages", []):
            output(f"@{m['from']} : {m['text']}")
            last_id = max(last_id, int(m["id"]))
        async for raw in ws:
            packet = json.loads(raw)
            if packet.get("type") == "message":
                m = packet["message"]
                if int(m["id"]) > last_id:
                    output(f"@{m['from']} : {m['text']}")
                    last_id = int(m["id"])
            elif packet.get("type") == "error": output("Erreur : " + str(packet.get("error")))

async def bridge(base, account, port, baud):
    try: import serial
    except ImportError: raise ApiError("pyserial absent. Installe requirements-messages.txt")
    if websockets is None: raise ApiError("websockets absent. Installe requirements-messages.txt")
    ser = serial.Serial(port, baudrate=baud, timeout=0.2, write_timeout=2)
    state = load_state(); seen = set()
    async def write_device(packet):
        try:
            frame = serial_packet(packet)
            await asyncio.to_thread(ser.write, frame)
        except Exception as exc:
            print("USB écriture :", exc, file=sys.stderr)
    async def send_device_message(m):
        mid = int(m["id"])
        if mid in seen or mid <= state.get("last_id", 0): return
        await write_device({"v":1,"type":"deliver","id":mid,"from":m["from"],"text":m["text"],"sent_at":m["sent_at"]})
        seen.add(mid); state["last_id"] = max(mid, state.get("last_id", 0)); save_state(state)
    print(f"Compagnon actif sur {port} ({baud} bauds). Ctrl+C pour quitter.")
    async with websockets.connect(ws_url(base), additional_headers={"Authorization":"Bearer "+account["token"]}, ping_interval=20, max_size=8192) as ws:
        hello = json.loads(await ws.recv())
        await write_device({"v":1,"type":"status","online":True,"username":hello.get("username")})
        # Flush stored messages first. WebSocket is already open, avoiding a gap.
        inbox = api(base, "/v1/inbox?after_id=" + str(state.get("last_id", 0)), account["token"])
        for m in inbox.get("messages", []): await send_device_message(m)
        async def serial_reader():
            while True:
                raw = await asyncio.to_thread(ser.readline)
                if not raw: continue
                if len(raw) > MAX_LINE or not raw.endswith(b"\n"):
                    await write_device({"v":1,"type":"error","error":"Trame USB invalide ou trop longue"}); continue
                try:
                    to, text, cid = parse_device_line(raw.strip())
                    result = await asyncio.to_thread(api, base, "/v1/messages", account["token"],
                                                     {"to":to,"text":text,"client_id":cid})
                    await write_device({"v":1,"type":"sent","client_id":cid,"message_id":result["message"]["id"]})
                except (ValueError, ApiError) as exc:
                    await write_device({"v":1,"type":"error","error":str(exc)[:180]})
        async def network_reader():
            async for raw in ws:
                packet = json.loads(raw)
                if packet.get("type") == "message": await send_device_message(packet["message"])
        try: await asyncio.gather(serial_reader(), network_reader())
        finally:
            await write_device({"v":1,"type":"status","online":False})
            ser.close()

def main():
    p = argparse.ArgumentParser(description="STI2D Messages — compagnon PC")
    p.add_argument("--server", default=os.environ.get("STI2D_MESSAGES_SERVER", "http://127.0.0.1:8765"))
    sub = p.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("register"); r.add_argument("username"); r.add_argument("--invite", default=os.environ.get("STI2D_INVITE_CODE"))
    l = sub.add_parser("login"); l.add_argument("username")
    s = sub.add_parser("send"); s.add_argument("to"); s.add_argument("text")
    sub.add_parser("listen")
    sub.add_parser("logout")
    b = sub.add_parser("bridge"); b.add_argument("--port", required=True); b.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()
    try:
        if args.cmd in ("register", "login"):
            password = getpass.getpass("Mot de passe (12 caractères minimum) : ")
            payload = {"username":args.username,"password":password}
            if args.cmd == "register": payload["invite_code"] = args.invite or getpass.getpass("Code d'invitation : ")
            result = api(args.server, "/v1/"+args.cmd, payload=payload)
            save_account(result["username"], result["access_token"])
            print("Compte prêt : @"+result["username"]+". Jeton stocké localement.")
        elif args.cmd == "send":
            a = account_or_exit()
            result = api(args.server, "/v1/messages", a["token"], {"to":args.to,"text":args.text,"client_id":secrets.token_hex(16)})
            print("Message envoyé (#"+str(result["message"]["id"])+")")
        elif args.cmd == "listen": asyncio.run(listen(args.server, account_or_exit()))
        elif args.cmd == "logout":
            a = account_or_exit()
            api(args.server, "/v1/logout", a["token"], {})
            try: TOKEN_FILE.unlink()
            except OSError: pass
            print("Session révoquée sur le serveur.")
        elif args.cmd == "bridge": asyncio.run(bridge(args.server, account_or_exit(), args.port, args.baud))
    except KeyboardInterrupt: print("\nDéconnecté.")
    except (ApiError, OSError) as exc: print("Erreur :", exc, file=sys.stderr); return 1
    return 0

if __name__ == "__main__": raise SystemExit(main())
