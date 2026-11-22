#!/usr/bin/env sh
# Starts the STI2D-OS portal: http://localhost:8000/web/ (and a phone address on the same Wi-Fi)
cd "$(dirname "$0")" || exit 1
exec python3 scripts/serve.py "${1:-8000}"
