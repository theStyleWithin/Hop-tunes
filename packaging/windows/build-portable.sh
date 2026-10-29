#!/usr/bin/env bash
# Makes the single-file portable exe: the static launcher, then the staged app as a zip, then a
# 16-byte footer ("HOPTPAY1" + the zip size as a little-endian 64-bit number).
# Run inside an MSYS2 shell:   build-portable.sh <launcher.exe> <stage-dir> <out.exe>
set -euo pipefail
LAUNCHER=${1:?usage: build-portable.sh <launcher.exe> <stage-dir> <out.exe>}
STAGE=${2:?}
OUT=${3:?}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

(cd "$STAGE" && zip -q -r -9 -X "$work/payload.zip" .)
size=$(stat -c %s "$work/payload.zip")
{
    cat "$LAUNCHER" "$work/payload.zip"
    printf 'HOPTPAY1'
    perl -e 'print pack("Q<", $ARGV[0])' "$size"
} > "$OUT"
echo "portable exe: $(stat -c %s "$OUT") bytes (payload $size)"
