#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/ray-scene-tracer"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

make -C "$ROOT" >/dev/null

BAD_SCENE="$TMP/bad.rt"
BAD_OUT="$TMP/bad.ppm"
cat >"$BAD_SCENE" <<'SCENE'
R 16 8
C 0,0,3 0,0,-1 60
not_a_shape 1 2 3
SCENE

if "$BIN" "$BAD_SCENE" "$BAD_OUT" >"$TMP/bad.stdout" 2>"$TMP/bad.stderr"; then
    echo "expected parser failure for unknown directive" >&2
    exit 1
fi
if [[ -s "$BAD_OUT" ]]; then
    echo "parser failure should not leave a rendered image" >&2
    exit 1
fi

SCENE_FILE="$TMP/smoke.rt"
cat >"$SCENE_FILE" <<'SCENE'
# Educational miniRT-style scene. RGB uses 0..255.
R 64 32
A 0.12 255,255,255
C 0,0.8,3.2 0,0.25,-1 55
L -3,5,2.5 0.9 255,244,220
sp 0,0,-1.4 1.10 220,70,45
sp 0.9,-0.1,-2.2 0.90 65,120,220
pl 0,-0.65,0 0,1,0 185,190,178
SCENE

PPM_ONE="$TMP/one.ppm"
PPM_TWO="$TMP/two.ppm"
CHECK_ONE=$("$BIN" "$SCENE_FILE" "$PPM_ONE" --checksum)
CHECK_TWO=$("$BIN" "$SCENE_FILE" "$PPM_TWO" --checksum)

mapfile -t HEADER < <(head -n 3 "$PPM_ONE")
if [[ "${HEADER[0]}" != "P3" ]]; then
    echo "expected P3 PPM magic, got ${HEADER[0]}" >&2
    exit 1
fi
if [[ "${HEADER[1]}" != "64 32" ]]; then
    echo "expected PPM dimensions 64 32, got ${HEADER[1]}" >&2
    exit 1
fi
if [[ "${HEADER[2]}" != "255" ]]; then
    echo "expected PPM max channel 255, got ${HEADER[2]}" >&2
    exit 1
fi

if [[ ! "$CHECK_ONE" =~ ^[0-9a-f]{16}$ ]]; then
    echo "expected checksum hex output, got $CHECK_ONE" >&2
    exit 1
fi
if [[ "$CHECK_ONE" != "$CHECK_TWO" ]]; then
    echo "checksum output is not deterministic" >&2
    exit 1
fi
if ! cmp -s "$PPM_ONE" "$PPM_TWO"; then
    echo "PPM output is not deterministic" >&2
    exit 1
fi

echo "render smoke checks passed"
