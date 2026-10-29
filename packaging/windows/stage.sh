#!/usr/bin/env bash
# Collects hop-tunes.exe and everything it needs to run on a PC with nothing installed into one
# folder: the Qt libraries, plugins and QML modules, the GStreamer plugins the app uses, libmpv, and
# every DLL they depend on. The installer and the portable exe are both made from that folder.
#
# Run inside an MSYS2 UCRT64 shell:   packaging/windows/stage.sh <build-dir> <stage-dir>
set -euo pipefail

BUILD=${1:?usage: stage.sh <build-dir> <stage-dir>}
STAGE=${2:?usage: stage.sh <build-dir> <stage-dir>}
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
PREFIX=${MSYSTEM_PREFIX:?run this in an MSYS2 shell}   # /ucrt64

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/hop-tunes.exe" "$STAGE/"

# ---- Qt: windeployqt copies the Qt DLLs, platform/style/image plugins and the QML modules the app imports.
echo "== windeployqt"
windeployqt6 --release --no-translations --qmldir "$(cygpath -w "$SRC/src/ui/qml")" \
    --dir "$(cygpath -w "$STAGE")" "$(cygpath -w "$STAGE/hop-tunes.exe")" || echo "note: windeployqt reported errors"

QT_ROOT=$PREFIX/share/qt6
# If windeployqt could not scan the QML imports, copy the modules the UI uses by hand.
for m in QtQml QtQuick; do
    [ -d "$STAGE/qml/$m" ] || { mkdir -p "$STAGE/qml"; cp -r "$QT_ROOT/qml/$m" "$STAGE/qml/"; echo "note: copied qml/$m by hand"; }
done
[ -d "$STAGE/qml/Qt" ] || { [ -d "$QT_ROOT/qml/Qt" ] && cp -r "$QT_ROOT/qml/Qt" "$STAGE/qml/"; } || true
# The offscreen platform lets CI start the real UI without a display; it is tiny.
for p in qoffscreen; do
    [ -e "$STAGE/platforms/$p.dll" ] || { mkdir -p "$STAGE/platforms"; cp "$QT_ROOT/plugins/platforms/$p.dll" "$STAGE/platforms/" 2>/dev/null || true; }
done
# JPEG, WebP, GIF... for the pictures section and album art.
mkdir -p "$STAGE/imageformats"
for f in "$QT_ROOT"/plugins/imageformats/q{jpeg,webp,gif,ico,tiff,svg}.dll; do [ -e "$f" ] && cp -n "$f" "$STAGE/imageformats/" || true; done

# ---- GStreamer: only the plugins Hop-tunes uses.
echo "== GStreamer plugins"
GST_PLUGINS="coreelements playback typefindfunctions audioconvert audioresample volume
  autodetect directsound wasapi wasapi2 app wavparse flac audioparsers id3demux apetag ogg vorbis opus
  isomp4 libav mpg123 lame wavpack asf matroska aiff equalizer replaygain spectrum level audiorate
  audiofx scaletempo interleave videoconvertscale videoconvert videoscale videoparsersbad avi mpegtsdemux mpegpsdemux
  gio qml6 opengl"
mkdir -p "$STAGE/lib/gstreamer-1.0" "$STAGE/libexec/gstreamer-1.0"
for p in $GST_PLUGINS; do
    f="$PREFIX/lib/gstreamer-1.0/libgst$p.dll"
    if [ -e "$f" ]; then cp "$f" "$STAGE/lib/gstreamer-1.0/"; else echo "note: GStreamer plugin '$p' not available"; fi
done
cp "$PREFIX/libexec/gstreamer-1.0/gst-plugin-scanner.exe" "$STAGE/libexec/gstreamer-1.0/"

# ---- libmpv, the video engine.
cp "$PREFIX"/bin/libmpv-2.dll "$STAGE/" 2>/dev/null || echo "note: libmpv-2.dll not found (video falls back to GStreamer)"

# ---- Every DLL those need, from the MSYS2 prefix, until nothing new turns up.
echo "== dependent DLLs"
queue=()
while IFS= read -r f; do queue+=("$f"); done < <(find "$STAGE" -type f \( -name '*.dll' -o -name '*.exe' \))
copied=0
while ((${#queue[@]})); do
    f=${queue[0]}; queue=("${queue[@]:1}")
    while IFS= read -r dll; do
        [ -n "$dll" ] || continue
        name=$(basename "$dll")
        [ -e "$STAGE/$name" ] && continue
        cp "$dll" "$STAGE/"
        queue+=("$STAGE/$name")
        copied=$((copied + 1))
    done < <(ldd "$f" 2>/dev/null | awk -v p="$PREFIX/" 'index($3, p) == 1 { print $3 }')
done
echo "copied $copied dependent DLLs"

# ---- Metadata and licences.
cp "$HERE/qt.conf" "$STAGE/qt.conf"
cp "$SRC/LICENSE" "$SRC/THIRD_PARTY_NOTICES.md" "$STAGE/"
cp "$SRC/README.md" "$STAGE/README.md"
echo "== staged $(find "$STAGE" -type f | wc -l) files, $(du -sm "$STAGE" | cut -f1) MB"
