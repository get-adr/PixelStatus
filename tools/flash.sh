#!/usr/bin/env bash
# Flasht eine mit tools/build-docker.sh gebaute Firmware vom Mac aus.
#
# Der Upload laeuft bewusst NICHT im Container: USB-Geraete lassen sich auf
# macOS nicht in die Linux-VM durchreichen. Noetig ist dafuer aber auch kein
# Compiler -- esptool ist Python und laeuft nativ auf Apple Silicon
# (brew install esptool).
#
#   tools/flash.sh                                   # d1_mini, Port automatisch
#   tools/flash.sh nodemcuv2                         # andere Env
#   tools/flash.sh d1_mini /dev/cu.usbserial-10      # Port explizit
set -euo pipefail

ENV_NAME="${1:-d1_mini}"
PORT="${2:-}"
BAUD="${BAUD:-115200}"   # hoehere Raten brachen beim D1 mini regelmaessig ab

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/.pio/build/$ENV_NAME/firmware.bin"

if [ ! -f "$BIN" ]; then
  echo "$BIN fehlt -- zuerst tools/build-docker.sh $ENV_NAME ausfuehren." >&2
  exit 1
fi
if ! command -v esptool >/dev/null 2>&1; then
  echo "esptool nicht gefunden (brew install esptool)." >&2
  exit 1
fi

if [ -z "$PORT" ]; then
  # Erster serieller Port, der wie ein USB-UART aussieht (CH340/CP210x/FTDI).
  PORT="$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1 || true)"
  [ -n "$PORT" ] || { echo "Kein USB-Seriell-Port gefunden, Port als 2. Argument angeben." >&2; exit 1; }
  echo "Port: $PORT"
fi

# keep: Flash-Modus, -Takt und -Groesse stehen bereits im Image-Header, den
# elf2bin passend zum Board erzeugt hat -- nicht mit eigenen Werten ueberschreiben.
exec esptool --port "$PORT" --baud "$BAUD" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep 0x0 "$BIN"
