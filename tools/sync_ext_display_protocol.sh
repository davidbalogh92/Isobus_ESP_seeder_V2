#!/usr/bin/env bash
# A külső kijelző protokoll fejlécének és specifikációjának frissítése a kijelző repóból.
# A fejléc HITELES példánya: github.com/davidbalogh92/Isobus_ExtDisplay_esp32 (protocol/).
# Itt VÁLTOZTATÁS NÉLKÜL tartjuk; a CI (tests/python) ellenőrzi, hogy a rögzített ellenőrzőösszeg egyezik.
#   tools/sync_ext_display_protocol.sh [kijelző-repó útvonal] [ág]
set -euo pipefail
SRC="${1:-../isobus_extdisplay_esp32}"
BRANCH="${2:-alap-firmware}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
git -C "$SRC" fetch -q origin "$BRANCH" && git -C "$SRC" checkout -q "origin/$BRANCH"
cp "$SRC/protocol/ext_display_protocol.h" "$ROOT/protocol/ext_display_protocol.h"
cp "$SRC/docs/ext_display_spec.md" "$ROOT/docs/ext_display_spec.md"
sha256sum "$ROOT/protocol/ext_display_protocol.h" | cut -d' ' -f1 > "$ROOT/protocol/ext_display_protocol.sha256"
echo "Frissítve: $(cat "$ROOT/protocol/ext_display_protocol.sha256") ($(git -C "$SRC" rev-parse --short HEAD))"
