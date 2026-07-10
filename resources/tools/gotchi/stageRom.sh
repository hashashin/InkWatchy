#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$TOOLS_DIR/../.." && pwd)"

CONFIG="$ROOT_DIR/src/defines/config.h"
SOURCE="$ROOT_DIR/resources/personal/gotchi/tama.b"
DEST_DIR="$TOOLS_DIR/fs/littlefs/other"
DEST="$DEST_DIR/tama.b"
PACKER="$SCRIPT_DIR/packRom.py"
PACKED_SIZE=9216
WORD_SIZE=12288

mkdir -p "$DEST_DIR"

if ! grep -Eq '^[[:space:]]*#define[[:space:]]+GOTCHI[[:space:]]+1([[:space:]]|$)' "$CONFIG"; then
    rm -f "$DEST"
    echo "[gotchi] GOTCHI is disabled; ROM omitted"
    exit 0
fi

if [[ ! -f "$SOURCE" ]]; then
    rm -f "$DEST"
    echo "[gotchi] ROM not found; place it at: $SOURCE"
    exit 0
fi

actual_size=$(stat -c '%s' "$SOURCE")
TEMP_DEST="$DEST.tmp"
rm -f "$DEST" "$TEMP_DEST"
trap 'rm -f "$TEMP_DEST"' EXIT

case "$actual_size" in
    "$PACKED_SIZE")
        cp -f "$SOURCE" "$TEMP_DEST"
        echo "[gotchi] Staged packed 12-bit ROM as /other/tama.b"
        ;;
    "$WORD_SIZE")
        "$PACKER" "$SOURCE" "$TEMP_DEST"
        echo "[gotchi] Converted 16-bit big-endian ROM and staged it as /other/tama.b"
        ;;
    *)
        rm -f "$DEST"
        echo "[gotchi] Invalid ROM size: $actual_size bytes (expected $PACKED_SIZE or $WORD_SIZE)" >&2
        exit 1
        ;;
esac

if [[ $(stat -c '%s' "$TEMP_DEST") -ne "$PACKED_SIZE" ]]; then
    echo "[gotchi] Staged ROM has an invalid size" >&2
    exit 1
fi

mv "$TEMP_DEST" "$DEST"
trap - EXIT
