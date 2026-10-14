#!/bin/sh
# Build the libarchive-based cabinet extractor. Needs gcc and libarchive.so.13 (no -dev headers required).
set -e
cd "$(dirname "$0")"
gcc -O2 -Wall -o xarc xarc.c -l:libarchive.so.13
echo "built $(pwd)/xarc"
