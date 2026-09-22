#!/usr/bin/env bash
# Baut die Firmware in einem arm64-Linux-Container (siehe tools/Dockerfile.firmware
# fuer die Begruendung). Die fertige .bin landet ueber das gemountete Projekt-
# verzeichnis direkt auf dem Mac und wird von dort mit tools/flash.sh geflasht --
# USB-Geraete lassen sich auf macOS nicht in die Linux-VM durchreichen, der
# Upload braucht aber ohnehin keinen Compiler.
#
#   tools/build-docker.sh              # Default-Env d1_mini
#   tools/build-docker.sh nodemcuv2    # andere Env
#   tools/build-docker.sh d1_mini -t clean
set -euo pipefail

ENV_NAME="${1:-d1_mini}"
shift || true

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="pixelstatus-firmware"
CACHE_VOLUME="pixelstatus-pio-cache"   # Framework + Toolchain, ~250 MB

# OrbStack legt die CLI nicht in den globalen PATH, Docker Desktop schon --
# beides abdecken, statt eine bestimmte Runtime vorauszusetzen.
if ! command -v docker >/dev/null 2>&1 && [ -x "$HOME/.orbstack/bin/docker" ]; then
  PATH="$HOME/.orbstack/bin:$PATH"
fi
if ! command -v docker >/dev/null 2>&1; then
  echo "Keine docker-CLI gefunden. OrbStack (oder Docker Desktop/colima) starten." >&2
  exit 1
fi

if [ ! -f "$ROOT/include/config.h" ]; then
  echo "include/config.h fehlt -- einmalig aus include/config.example.h anlegen." >&2
  exit 1
fi

docker build -q -t "$IMAGE" -f "$ROOT/tools/Dockerfile.firmware" "$ROOT/tools" >/dev/null
exec docker run --rm -t \
  -v "$ROOT:/project" \
  -v "$CACHE_VOLUME:/pio" \
  "$IMAGE" pio run -e "$ENV_NAME" "$@"
