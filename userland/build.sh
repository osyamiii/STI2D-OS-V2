#!/bin/bash
# STI2D-OS userland: build for one or more models, both slots.
#
#   userland/build.sh                 n0120 (default)
#   userland/build.sh n0110 n0115 n0120
#   APP_VERSION=26.4.0 userland/build.sh n0120   (must equal the calculator's official version)
#
# Output: userland/firmware/<model>/sti2d-os.{A,B}.{dfu,elf}
# Needs: GNU make >= 4.3, arm-none-eabi-gcc, Python 3 with the packages of
# epsilon-src/epsilon (pypng, lz4, ...). See BUILD.md.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/epsilon-src/epsilon"
MAKE_BIN="${MAKE_BIN:-$(command -v gmake || true)}"
[ -x /opt/homebrew/opt/make/libexec/gnubin/make ] && MAKE_BIN=/opt/homebrew/opt/make/libexec/gnubin/make
MAKE_BIN="${MAKE_BIN:-make}"
APP_VERSION="${APP_VERSION:-26.4.0}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"
MODELS=("$@")
[ ${#MODELS[@]} -eq 0 ] && MODELS=(n0120)

# the shell's sources live in firmware/: the userland tree points to them
ln -sfn ../../../../../firmware/sti2d-play-source-v1 "$SRC/apps/sti2d/play"
ln -sfn ../../../../../firmware/v2 "$SRC/apps/sti2d/v2"

for m in "${MODELS[@]}"; do
  echo "== STI2D-OS userland $m (Epsilon $APP_VERSION)"
  (cd "$SRC" && "$MAKE_BIN" -j"$JOBS" PLATFORM="$m" APP_VERSION="$APP_VERSION" \
      userland.allow3rdparty.A.dfu userland.allow3rdparty.B.dfu)
  mkdir -p "$HERE/firmware/$m"
  for s in A B; do
    cp "$SRC/output/release/$m/userland.allow3rdparty.$s.dfu" "$HERE/firmware/$m/sti2d-os.$s.dfu"
    arm-none-eabi-objcopy --strip-debug "$SRC/output/release/$m/userland.allow3rdparty.$s.elf" "$HERE/firmware/$m/sti2d-os.$s.elf"
  done
  arm-none-eabi-size "$HERE/firmware/$m/sti2d-os.B.elf"
done
