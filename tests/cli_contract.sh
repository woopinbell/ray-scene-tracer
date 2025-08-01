#!/usr/bin/env bash
set -euo pipefail

BIN=$1
ROOT=$2
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

expect_usage_error() {
    set +e
    "$BIN" "$@" >"$TMP/stdout" 2>"$TMP/stderr"
    STATUS=$?
    set -e
    if [[ $STATUS -ne 2 ]]; then
        echo "expected exit 2 for: $*" >&2
        exit 1
    fi
    if ! grep -q '^usage: ray-scene-tracer ' "$TMP/stderr"; then
        echo "expected usage for: $*" >&2
        exit 1
    fi
}

SCENE="$ROOT/scenes/basic.rt"
OUT="$TMP/output.ppm"

expect_usage_error
expect_usage_error "$SCENE"
expect_usage_error "$SCENE" "$OUT" --unknown
expect_usage_error "$SCENE" "$OUT" --checksum --checksum
expect_usage_error "$SCENE" "$OUT" --accel
expect_usage_error "$SCENE" "$OUT" --accel tree
expect_usage_error "$SCENE" "$OUT" --accel bvh --accel linear
expect_usage_error "$SCENE" "$OUT" --threads
expect_usage_error "$SCENE" "$OUT" --threads 0
expect_usage_error "$SCENE" "$OUT" --threads -1
expect_usage_error "$SCENE" "$OUT" --threads many
expect_usage_error "$SCENE" "$OUT" --threads 18446744073709551616
expect_usage_error "$SCENE" "$OUT" --threads 1 --threads auto
expect_usage_error "$SCENE" "$OUT" --max-depth
expect_usage_error "$SCENE" "$OUT" --max-depth -1
expect_usage_error "$SCENE" "$OUT" --max-depth 33
expect_usage_error "$SCENE" "$OUT" --max-depth 4x
expect_usage_error "$SCENE" "$OUT" --max-depth 4 --max-depth 5

CHECKSUM=$(
    "$BIN" "$SCENE" "$OUT" \
        --threads 1 \
        --max-depth 0 \
        --accel linear \
        --checksum
)
if [[ ! "$CHECKSUM" =~ ^[0-9a-f]{16}$ || ! -s "$OUT" ]]; then
    echo "valid options did not produce an image and checksum" >&2
    exit 1
fi

AUTO_OUT="$TMP/auto.ppm"
AUTO_CHECKSUM=$(
    "$BIN" "$SCENE" "$AUTO_OUT" \
        --threads auto \
        --max-depth 32 \
        --accel bvh \
        --checksum
)
if [[ "$AUTO_CHECKSUM" != "$CHECKSUM" || ! -s "$AUTO_OUT" ]]; then
    echo "boundary options changed the diffuse render" >&2
    exit 1
fi

echo "CLI contract checks passed"
