#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"          # resources/tools
ROOT_DIR="$(cd "$TOOLS_DIR/../.." && pwd)"         # repo root

SRC="$ROOT_DIR/resources/personal/qrapp/qrlist.txt"
DST_DIR="$TOOLS_DIR/fs/littlefs/qrapp"
DST="$DST_DIR/qrlist.txt"

echo "[qrapp] SRC: $SRC"
echo "[qrapp] DST: $DST"

mkdir -p "$DST_DIR"

if [[ ! -f "$SRC" ]]; then
  echo "[qrapp] Missing source file, skipping"
  exit 0
fi

cp -f "$SRC" "$DST"
echo "[qrapp] OK"
ls -la "$DST"
