#!/bin/sh
# Unpack ZunePackage.exe end to end:  SFX -> outer CAB -> product MSI -> embedded CAB -> real install tree.
# Usage: unpack_all.sh ZunePackage.exe <workdir> [x86|x64]
set -e
T="$(cd "$(dirname "$0")" && pwd)"; IN="$1"; W="$2"; ARCH="${3:-x86}"
[ -x "$T/xarc" ] || "$T/build.sh"
mkdir -p "$W"
python3 -I "$T/sfxcab.py" "$IN" "$W/payload.cab"
"$T/xarc" "$W/payload.cab" "$W/pkg"
python3 -I "$T/msiread.py" "$W/pkg/$ARCH/packages/zune-$ARCH.msi" "$W/msi"
"$T/xarc" "$W/msi/streams/Zune.cab" "$W/cab"
python3 -I "$T/layout.py" "$W/msi/tables" "$W/cab" "$W/tree"
[ -d "$W/tree/TARGETDIR" ] && mv "$W/tree/TARGETDIR"/* "$W/tree/" && rmdir "$W/tree/TARGETDIR"
echo "done: install tree in $W/tree, MSI tables in $W/msi/tables"
