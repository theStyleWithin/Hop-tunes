#!/usr/bin/env bash
# Installs everything needed to build, test and run Hop-tunes inside the dev container / Codespace.
set -euo pipefail
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake ninja-build g++ pkg-config gdb \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtquick-dialogs qml6-module-qtquick-templates \
  qml6-module-qtqml-workerscript qml6-module-qtcore \
  libtag1-dev libsqlite3-dev libmpv-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav \
  gstreamer1.0-pulseaudio
echo
echo "Ready. Build with:  cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build"
echo "Run the app in the browser desktop (Ports tab -> 6080):  DISPLAY=:1 ./build/hop-tunes"
