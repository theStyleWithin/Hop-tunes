#!/bin/sh
# Smoke test for a Hop-tunes AppImage, meant to run inside a fresh distro container.
# Usage: smoke-test.sh <path/to/Hop-tunes.AppImage> <out-dir> <label>
# Env:   HOPTUNES_TEST_PLATFORM=offscreen (default) | xcb | wayland  (display server must already run)
# Checks: the AppImage starts, the UI loads, the bundled TagLib scans a song, the bundled
# GStreamer actually plays it, a local profile is created, and a screenshot is written. Exits non-zero on any failure.
set -u
APPIMAGE=$(readlink -f "$1"); OUT=$(readlink -f "$2"); LABEL=$3
mkdir -p "$OUT"
WORK=$(mktemp -d); export HOME="$WORK/home"; mkdir -p "$HOME/Music/Test Artist/Test Album"

# 10 s silent PCM WAV (8 kHz, mono, 16-bit), written with POSIX printf only.
le32() { printf "\\$(printf %03o $(( $1 & 255 )))\\$(printf %03o $(( ($1 >> 8) & 255 )))\\$(printf %03o $(( ($1 >> 16) & 255 )))\\$(printf %03o $(( ($1 >> 24) & 255 )))"; }
le16() { printf "\\$(printf %03o $(( $1 & 255 )))\\$(printf %03o $(( ($1 >> 8) & 255 )))"; }
WAV="$HOME/Music/Test Artist/Test Album/01 Silence.wav"
DATA=160000
{ printf RIFF; le32 $((36 + DATA)); printf WAVEfmt\ ; le32 16; le16 1; le16 1; le32 8000; le32 16000; le16 2; le16 16
  printf data; le32 $DATA; head -c $DATA /dev/zero; } > "$WAV"

cd "$WORK"
LOG="$OUT/$LABEL.log"
QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM="${HOPTUNES_TEST_PLATFORM:-offscreen}" QT_QUICK_BACKEND=software \
HOPTUNES_AUDIO_SINK="fakesink sync=true" HOPTUNES_AUTOPLAY=0 HOPTUNES_SCREENSHOT="$OUT/$LABEL.png" \
HOPTUNES_PROFILE="Smoke Test" timeout 120 "$APPIMAGE" --appimage-extract-and-run > "$LOG" 2>&1
CODE=$?
cat "$LOG"

diagnose() {
  echo "---- diagnostics [$LABEL]"
  "$APPIMAGE" --appimage-version 2>&1 | head -3
  ( cd "$WORK" && "$APPIMAGE" --appimage-extract >/dev/null 2>&1 ) || echo "extract failed"
  R="$WORK/squashfs-root"
  if [ -d "$R" ]; then
    echo "-- missing libraries for the main binary:"
    LD_LIBRARY_PATH="$R/usr/lib" ldd "$R/usr/bin/hop-tunes" 2>&1 | grep -i "not found" || echo "   (none)"
    echo "-- traced start (AppRun with hooks):"
    APPDIR="$R" APPIMAGE="$APPIMAGE" QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen \
      HOPTUNES_SCREENSHOT="$WORK/diag.png" timeout 60 bash -x "$R/AppRun" > "$WORK/trace.log" 2>&1
    echo "   AppRun exit code: $?"
    grep -v "^+++* *\(for\|\[\|_d=\|_ht_dir=\)" "$WORK/trace.log" | tail -60
  fi
}
fail() { echo "SMOKE FAIL [$LABEL]: $1"; [ "${DIAGNOSED:-0}" = 1 ] || { DIAGNOSED=1; diagnose; }; exit 1; }
[ $CODE -eq 0 ] || fail "exit code $CODE"
grep -q "hop-tunes selftest:" "$LOG" || fail "no selftest line (app did not finish starting)"
grep -q 'profile="Smoke Test"' "$LOG" || fail "profile was not created/opened"
[ -f "$HOME/.local/share/hop-tunes/profiles/profiles.json" ] || fail "profiles.json not written"
grep -q "songs=1 " "$LOG" || fail "library scan did not find the test song"
grep -q "playing=1 " "$LOG" || fail "bundled GStreamer did not play the test song"
grep -q 'error=""' "$LOG" || fail "player reported an error"
grep -q "screenshot=1" "$LOG" || fail "screenshot not saved"
if grep -Eq "QQmlApplicationEngine failed|module .* is not installed|Cannot load library|could not load the Qt platform plugin|undefined symbol|GLIBC_[0-9.]+' not found" "$LOG"; then
  fail "error messages in log"
fi
echo "SMOKE OK [$LABEL]"
