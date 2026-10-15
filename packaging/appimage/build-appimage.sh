#!/bin/bash
# Builds Hop-tunes-<version>-x86_64.AppImage.
#
# Run on the OLDEST distro you want to support (CI uses Ubuntu 22.04, glibc 2.35): an
# AppImage runs on systems whose glibc is the same or newer than the build machine's.
# Expects Qt 6.8 (qmake in PATH), GStreamer + plugins, TagLib and SQLite installed.
#
# Usage: packaging/appimage/build-appimage.sh [version]
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
VERSION=${1:-$(sed -n 's/^project(hop-tunes VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")}
BUILD=$ROOT/build-appimage
APPDIR=$ROOT/AppDir
TOOLS=$ROOT/.appimage-tools
APP_ID=io.github.thestylewithin.Hop_tunes
ARCH_LIB=/usr/lib/x86_64-linux-gnu

echo "== Hop-tunes $VERSION"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD"
HOPTUNES_AUDIO_SINK=fakesink QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD" --output-on-failure

rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD"

# ---- tools -----------------------------------------------------------------------------
mkdir -p "$TOOLS"
fetch() { [ -s "$TOOLS/$2" ] || curl -fsSL --retry 3 -o "$TOOLS/$2" "$1"; chmod +x "$TOOLS/$2"; }
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage linuxdeploy
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt
fetch https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gstreamer/master/linuxdeploy-plugin-gstreamer.sh linuxdeploy-plugin-gstreamer.sh
export PATH="$TOOLS:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1          # no FUSE needed on the build machine

# ---- Qt: QML modules our UI imports, plus every display backend ---------------------------
export QML_SOURCES_PATHS="$ROOT/src/ui/qml"
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so;libqwayland-egl.so;libqwayland-generic.so"
export EXTRA_QT_MODULES="waylandclient"

# ---- GStreamer: bundle ONLY the audio plugins Hop-tunes uses (+ scanner) -----------------
# Fewer plugins = smaller AppImage, fewer dependencies, fewer ways to break on odd systems.
GST_PLUGINS="coreelements playback typefindfunctions audioconvert audioresample volume
  autodetect pulseaudio alsa app wavparse flac audioparsers id3demux apetag ogg vorbis opus
  isomp4 libav mpg123 wavpack asf matroska aiff equalizer replaygain spectrum level audiorate
  audiofx scaletempo interleave"
GST_CURATED=$(mktemp -d)
for p in $GST_PLUGINS; do
  if [ -e "$ARCH_LIB/gstreamer-1.0/libgst$p.so" ]; then ln -s "$ARCH_LIB/gstreamer-1.0/libgst$p.so" "$GST_CURATED/"
  else echo "note: GStreamer plugin '$p' not available on this build host"; fi
done
export GSTREAMER_PLUGINS_DIR="$GST_CURATED"
export GSTREAMER_HELPERS_DIR="$ARCH_LIB/gstreamer1.0/gstreamer-1.0"
export GSTREAMER_VERSION=1.0

linuxdeploy --appdir "$APPDIR" \
  --desktop-file "$APPDIR/usr/share/applications/$APP_ID.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/$APP_ID.svg" \
  --plugin qt --plugin gstreamer

# Wayland needs its shell/decoration plugins too, or Qt silently falls back to XWayland.
QT_PLUGINS=$(qmake -query QT_INSTALL_PLUGINS)
for d in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
  if [ -d "$QT_PLUGINS/$d" ]; then
    mkdir -p "$APPDIR/usr/plugins/$d"
    cp -a "$QT_PLUGINS/$d/." "$APPDIR/usr/plugins/$d/"
    for so in "$APPDIR/usr/plugins/$d"/*.so; do linuxdeploy --appdir "$APPDIR" --deploy-deps-only "$so"; done
  fi
done

# Host-first fallback libraries (see apprun-hooks/hop-tunes-fallback-libs.sh).
FALLBACK_LIBS="libOpenGL.so.0 libGLdispatch.so.0 libcom_err.so.2 libgpg-error.so.0 libfribidi.so.0"
for lib in $FALLBACK_LIBS; do
  src=$(ls "$ARCH_LIB/$lib" /lib/x86_64-linux-gnu/$lib 2>/dev/null | head -1 || true)
  if [ -n "$src" ]; then mkdir -p "$APPDIR/usr/lib/fallback/$lib"; cp -L "$src" "$APPDIR/usr/lib/fallback/$lib/"
  else echo "warning: fallback library $lib not found on build host"; fi
done
mkdir -p "$APPDIR/apprun-hooks"
cp "$ROOT/packaging/appimage/apprun-hooks/"*.sh "$APPDIR/apprun-hooks/"

# Report what the AppImage still expects from the host (for the compatibility notes).
echo "== libraries expected from the host system:"
{ find "$APPDIR/usr" -type f -name '*.so*' -o -type f -path '*/bin/*'; } 2>/dev/null \
  | xargs -r objdump -p 2>/dev/null | sed -n 's/^ *NEEDED *//p' | sort -u \
  | while read -r so; do [ -n "$(find "$APPDIR" -name "$so" -print -quit)" ] || echo "   $so"; done || true

# ---- pack ---------------------------------------------------------------------------------
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
export LDAI_UPDATE_INFORMATION="gh-releases-zsync|theStyleWithin|Hop-tunes|continuous|Hop-tunes-*-x86_64.AppImage.zsync"
linuxdeploy --appdir "$APPDIR" --output appimage

OUT=$(ls -1 "$ROOT"/Hop-tunes-*-x86_64.AppImage | head -1)
echo "== built $OUT ($(du -h "$OUT" | cut -f1))"
echo "glibc symbols required (highest):"
{ find "$APPDIR" -type f \( -name '*.so*' -o -perm -u+x \) -exec objdump -T {} + 2>/dev/null || true; } \
  | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1 || true
