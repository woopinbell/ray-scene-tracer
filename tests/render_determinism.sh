#!/usr/bin/env bash
set -euo pipefail

BIN=$1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

SCENE="$TMP/determinism.rt"
cat >"$SCENE" <<'SCENE'
R 96 54
A 0.12 255,255,255
C 0,1,-4 0,-0.08,1 55
L -3,6,-1 0.9 255,244,220
L 4,3,5 0.4 180,210,255
sp -0.9,0,3 1.5 220,70,45 diffuse
sp 0.9,0,3.5 1.4 210,220,235 metal
pl 0,-0.8,0 0,1,0 150,165,180 diffuse
cy 2,-0.1,6 0.2,1,0.1 0.8 2 70,190,120 diffuse
SCENE

render() {
    NAME=$1
    ACCEL=$2
    THREADS=$3
    "$BIN" "$SCENE" "$TMP/$NAME.ppm" \
        --accel "$ACCEL" \
        --threads "$THREADS" \
        --max-depth 4 \
        --checksum
}

LINEAR_ONE=$(render linear-one linear 1)
LINEAR_FOUR=$(render linear-four linear 4)
BVH_ONE=$(render bvh-one bvh 1)
BVH_FOUR=$(render bvh-four bvh 4)

for CHECKSUM in "$LINEAR_FOUR" "$BVH_ONE" "$BVH_FOUR"; do
    if [[ "$CHECKSUM" != "$LINEAR_ONE" ]]; then
        echo "render modes produced different checksums" >&2
        exit 1
    fi
done

for IMAGE in linear-four bvh-one bvh-four; do
    if ! cmp -s "$TMP/linear-one.ppm" "$TMP/$IMAGE.ppm"; then
        echo "render modes produced different PPM bytes: $IMAGE" >&2
        exit 1
    fi
done

echo "render output determinism checks passed"
