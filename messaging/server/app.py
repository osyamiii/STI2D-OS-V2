"""STI2D Messages relay API. Requires TLS termination in public deployments."""
from __future__ import annotations
import hashlib, hmac, os, re, secrets, sqlite3, time
from contextlib import asynccontextmanager, contextmanager
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, Header, HTTPException, WebSocket, WebSocketDisconnect
from pydantic import BaseModel, Field

DB_PATH = Path(os.environ.get("STI2D_MESSAGES_DB", "./sti2d-messages.sqlite3"))
INVITE_CODE = os.environ.get("STI2D_INVITE_CODE", "")
TOKEN_TTL = int(os.environ.get("STI2D_TOKEN_TTL", str(30 * 24 * 3600)))
MAX_TEXT = 1000
USERNAME_RE = re.compile(r"^[a-zA-Z0-9_]{3,24}$")

_live: dict[str, set[WebSocket]] = {}

@contextmanager
def db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(DB_PATH, timeout=10)
    con.row_factory = sqlite3.Row
    try:
        con.execute("PRAGMA journal_mode=WAL")
        yield con
        con.commit()
    finally:
        con.close()

def init_db():
    with db() as c:
        c.executescript("""
        CREATE TABLE IF NOT EXISTS users (
          id INTEGER PRIMARY KEY, username TEXT UNIQUE COLLATE NOCASE NOT NULL,
          salt BLOB NOT NULL, password_hash BLOB NOT NULL, created_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS tokens (
          token_hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id),
          expires_at INTEGER NOT NULL, created_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS messages (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          sender_id INTEGER NOT NULL REFERENCES users(id),
          recipient_id INTEGER NOT NULL REFERENCES users(id),
          client_id TEXT NOT NULL, body TEXT NOT NULL, created_at INTEGER NOT NULL,
          sender_deleted INTEGER NOT NULL DEFAULT 0,
          recipient_deleted INTEGER NOT NULL DEFAULT 0,
          UNIQUE(sender_id, client_id)
        );
        CREATE INDEX IF NOT EXISTS messages_recipient_id_id ON messages(recipient_id,id);
        """)
        # Add deletion flags when upgrading an earlier database.
        cols = {row[1] for row in c.execute("PRAGMA table_info(messages)").fetchall()}
        if "sender_deleted" not in cols:
            c.execute("ALTER TABLE messages ADD COLUMN sender_deleted INTEGER NOT NULL DEFAULT 0")
        if "recipient_deleted" not in cols:
            c.execute("ALTER TABLE messages ADD COLUMN recipient_deleted INTEGER NOT NULL DEFAULT 0")
        c.execute("DELETE FROM messages WHERE created_at < ?", (int(time.time()) - 30 * 24 * 3600,))

@asynccontextmanager
async def lifespan(application):
    init_db()
    yield

app = FastAPI(title="STI2D Messages Relay", version="1.0.0", docs_url=None, redoc_url=None, lifespan=lifespan)

class RegisterBody(BaseModel):
    username: str = Field(min_length=3, max_length=24)
    password: str = Field(min_length=12, max_length=128)
    invite_code: str = Field(min_length=1, max_length=128)

class LoginBody(BaseModel):
    username: str = Field(min_length=3, max_length=24)
    password: str = Field(min_length=1, max_length=128)

class SendBody(BaseModel):
    to: str = Field(min_length=3, max_length=24)
    text: str = Field(min_length=1, max_length=MAX_TEXT)
    client_id: str = Field(min_length=8, max_length=64)


def _password(password: str, salt: bytes) -> bytes:
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, 310_000)

def _new_token(user_id: int) -> str:
    raw = secrets.token_urlsafe(36)
    now = int(time.time())
    with db() as c:
        c.execute("INSERT INTO tokens(token_hash,user_id,expires_at,created_at) VALUES(?,?,?,?)",
                  (hashlib.sha256(raw.encode()).hexdigest(), user_id, now + TOKEN_TTL, now))
    return raw

def _user_for_token(raw: str) -> Optional[sqlite3.Row]:
    if not raw or len(raw) > 256:
        return None
    digest = hashlib.sha256(raw.encode()).hexdigest()
    with db() as c:
        return c.execute("""SELECT u.id,u.username FROM tokens t JOIN users u ON u.id=t.user_id
                            WHERE t.token_hash=? AND t.expires_at>?""", (digest, int(time.time()))).fetchone()

def _bearer(header: Optional[str]) -> sqlite3.Row:
    if not header or not header.startswith("Bearer "):
        raise HTTPException(401, "Authentification requise")
    user = _user_for_token(header[7:].strip())
    if user is None:
        raise HTTPException(401, "Jeton invalide ou expiré")
    return user

def _purge_expired(c: sqlite3.Connection):
    c.execute("DELETE FROM messages WHERE created_at < ?", (int(time.time()) - 30 * 24 * 3600,))

def _public_message(row: sqlite3.Row) -> dict:
    return {"id": row["id"], "from": row["sender"], "to": row["recipient"],
            "text": row["body"], "sent_at": row["created_at"]}

@app.get("/health")
def health():
    return {"status": "ok", "service": "sti2d-messages", "version": "1.0.0"}

@app.post("/v1/register")
def register(body: RegisterBody):
    if not INVITE_CODE:
        raise HTTPException(503, "Inscription désactivée : l'administrateur doit définir STI2D_INVITE_CODE")
    if not hmac.compare_digest(body.invite_code, INVITE_CODE):
        raise HTTPException(403, "Code d'invitation incorrect")
    if not USERNAME_RE.fullmatch(body.username):
        raise HTTPException(422, "Pseudo : 3 à 24 caractères, lettres, chiffres ou _")
    salt = secrets.token_bytes(16)
    try:
        with db() as c:
            cur = c.execute("INSERT INTO users(username,salt,password_hash,created_at) VALUES(?,?,?,?)",
                (body.username, salt, _password(body.password, salt), int(time.time())))
            user_id = cur.lastrowid
    except sqlite3.IntegrityError:
        raise HTTPException(409, "Ce pseudo est déjà utilisé")
    return {"username": body.username, "access_token": _new_token(user_id), "token_type": "bearer"}

@app.post("/v1/login")
def login(body: LoginBody):
    with db() as c:
        row = c.execute("SELECT id,username,salt,password_hash FROM users WHERE username=? COLLATE NOCASE",
                        (body.username,)).fetchone()
    if row is None or not hmac.compare_digest(_password(body.password, row["salt"]), row["password_hash"]):
        raise HTTPException(401, "Pseudo ou mot de passe incorrect")
    return {"username": row["username"], "access_token": _new_token(row["id"]), "token_type": "bearer"}

@app.get("/v1/me")
def me(authorization: Optional[str] = Header(default=None)):
    u = _bearer(authorization)
    return {"username": u["username"]}

@app.post("/v1/messages")
async def send(body: SendBody, authorization: Optional[str] = Header(default=None)):
    sender = _bearer(authorization)
    text = body.text.strip()
    if not text:
        raise HTTPException(422, "Le message est vide")
    now = int(time.time())
    with db() as c:
        _purge_expired(c)
        recipient = c.execute("SELECT id,username FROM users WHERE username=? COLLATE NOCASE", (body.to,)).fetchone()
        if recipient is None:
            raise HTTPException(404, "Destinataire inconnu")
        if recipient["id"] == sender["id"]:
            raise HTTPException(422, "Tu ne peux pas t'envoyer un message à toi-même")
        previous = c.execute("""SELECT id,recipient_id,body,created_at FROM messages
                                WHERE sender_id=? AND client_id=?""", (sender["id"], body.client_id)).fetchone()
        if previous is not None:
            if previous["recipient_id"] != recipient["id"] or previous["body"] != text:
                raise HTTPException(409, "Identifiant client déjà utilisé pour un autre message")
            return {"status": "sent", "message": {"id": previous["id"], "from": sender["username"],
                    "to": recipient["username"], "text": previous["body"], "sent_at": previous["created_at"]}}
        recent = c.execute("SELECT COUNT(*) FROM messages WHERE sender_id=? AND created_at> ?", (sender["id"], now-60)).fetchone()[0]
        if recent >= 30:
            raise HTTPException(429, "Limite atteinte : 30 messages par minute")
        cur = c.execute("INSERT INTO messages(sender_id,recipient_id,client_id,body,created_at) VALUES(?,?,?,?,?)",
                        (sender["id"], recipient["id"], body.client_id, text, now))
        message_id = cur.lastrowid
        msg = {"id": message_id, "from": sender["username"], "to": recipient["username"], "text": text, "sent_at": now}
    # Persist first, then push to every active recipient session.
    for socket in list(_live.get(recipient["username"].lower(), set())):
        try:
            await socket.send_json({"type": "message", "message": msg})
        except Exception:
            _live.get(recipient["username"].lower(), set()).discard(socket)
    return {"status": "sent", "message": msg}

@app.get("/v1/inbox")
def inbox(after_id: int = 0, limit: int = 50, authorization: Optional[str] = Header(default=None)):
    u = _bearer(authorization)
    limit = max(1, min(limit, 100))
    with db() as c:
        _purge_expired(c)
        rows = c.execute("""SELECT m.id,m.body,m.created_at,s.username sender,r.username recipient
                            FROM messages m JOIN users s ON s.id=m.sender_id
                            JOIN users r ON r.id=m.recipient_id
                            WHERE m.recipient_id=? AND m.recipient_deleted=0 AND m.id>? ORDER BY m.id LIMIT ?""",
                         (u["id"], max(0, after_id), limit)).fetchall()
    return {"messages": [_public_message(r) for r in rows]}

@app.delete("/v1/messages/{message_id}")
def delete_message(message_id: int, authorization: Optional[str] = Header(default=None)):
    u = _bearer(authorization)
    with db() as c:
        cur = c.execute("UPDATE messages SET sender_deleted=1 WHERE id=? AND sender_id=? AND sender_deleted=0",
                        (message_id, u["id"]))
        if cur.rowcount == 0:
            cur = c.execute("UPDATE messages SET recipient_deleted=1 WHERE id=? AND recipient_id=? AND recipient_deleted=0",
                            (message_id, u["id"]))
        if cur.rowcount == 0:
            raise HTTPException(404, "Message introuvable")
    return {"status": "deleted"}

@app.post("/v1/logout")
def logout(authorization: Optional[str] = Header(default=None)):
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(401, "Authentification requise")
    raw = authorization[7:].strip()
    digest = hashlib.sha256(raw.encode()).hexdigest()
    with db() as c:
        c.execute("DELETE FROM tokens WHERE token_hash=?", (digest,))
    return {"status": "logged_out"}

@app.websocket("/v1/live")
async def live(websocket: WebSocket):
    header = websocket.headers.get("authorization", "")
    user = _user_for_token(header[7:].strip()) if header.startswith("Bearer ") else None
    if user is None:
        await websocket.close(code=4401, reason="Unauthorized")
        return
    key = user["username"].lower()
    await websocket.accept()
    _live.setdefault(key, set()).add(websocket)
    try:
        await websocket.send_json({"type": "ready", "username": user["username"]})
        while True:
            # Client may send ping; message delivery is server-to-client only.
            packet = await websocket.receive_json()
            if packet.get("type") == "ping":
                await websocket.send_json({"type": "pong"})
            else:
                await websocket.send_json({"type": "error", "error": "Seuls les ping sont acceptés sur ce canal"})
    except WebSocketDisconnect:
        pass
    finally:
        _live.get(key, set()).discard(websocket)
        if not _live.get(key):
            _live.pop(key, None)
