#!/bin/sh
# STI2D-OS V2: the final link of STI2D-OS.nwa, done the way the calculator's installer does it
# (nwlink nwa-bin places the app in RAM and flash and resolves every symbol against EADK),
# for the software each model can run.
#
#   tools/sti2dos_check.sh MODEL NWA ARENA_EXTRA_FILE "NWLINK"
#
# N0120:        Epsilon >= 23.2   RAM 148928 bytes: the launcher and every linked game fit
# N0110/N0115:  Epsilon >= 23.2   same check
#               Upsilon (06/2026) RAM 107674 bytes: the launcher fits; a game needing more RAM
#                                 than Upsilon gives says so on its card (np_game_ram_missing)
# App space: 2031616 bytes (from 0x90200000, the latest start any supported software uses).
set -e
MODEL=$1
NWA=$2
EXTRA=$(cat "$3")
NWLINK=$4
DIR=$(dirname "$NWA")
RAM_EPSILON=148928
RAM_UPSILON=107674
APP_SPACE=2031616

if ! $NWLINK nwa-bin --ram-length $((RAM_EPSILON - EXTRA)) "$NWA" "$DIR/STI2D-OS.bin" >/dev/null 2>"$DIR/link.log"; then
  echo "STI2D-OS $MODEL: final link for Epsilon FAILED:"; cat "$DIR/link.log"; exit 1
fi
n=$(wc -c < "$DIR/STI2D-OS.bin" | tr -d ' ')
if [ "$n" -gt "$APP_SPACE" ]; then echo "STI2D-OS $MODEL: $n bytes, bigger than the app space ($APP_SPACE)"; exit 1; fi
echo "== STI2D-OS $MODEL / Epsilon >= 23.2: links, installs as $n bytes ($((APP_SPACE - n)) to spare), launcher + every game fit in $RAM_EPSILON bytes of RAM"

if [ "$MODEL" != n0120 ]; then
  if $NWLINK nwa-bin --ram-length $RAM_UPSILON "$NWA" "$DIR/STI2D-OS-Upsilon.bin" >/dev/null 2>"$DIR/link-upsilon.log"; then
    echo "== STI2D-OS $MODEL / Upsilon: links in $RAM_UPSILON bytes of RAM (games needing more show the RAM they miss)"
  else
    echo "STI2D-OS $MODEL / Upsilon: the launcher does not fit in $RAM_UPSILON bytes:"; cat "$DIR/link-upsilon.log"; exit 1
  fi
fi
