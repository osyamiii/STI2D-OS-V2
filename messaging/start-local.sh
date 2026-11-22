#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"
: "${STI2D_INVITE_CODE:?Définis STI2D_INVITE_CODE avec un code d'invitation avant de lancer le relais}"
export STI2D_MESSAGES_DB="${STI2D_MESSAGES_DB:-$ROOT/messaging/messages.sqlite3}"
exec python -m uvicorn messaging.server.app:app --host 127.0.0.1 --port "${STI2D_MESSAGES_PORT:-8765}"
