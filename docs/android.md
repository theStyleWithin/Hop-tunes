# Building the Android app shell

Hop-tunes can be built for Android (arm64-v8a, Android 7.0 / API 24 and up) as an **app shell**: it starts and
shows the library screens and **plays music** (through Qt Multimedia; no gapless or crossfade yet). **Video
playback and video thumbnails are not there yet** (GStreamer and libmpv are desktop-only), and Party Watch is
desktop-only. Navigation is a bottom bar or a side drawer (settings › navigation), and a side rail or a
permanent drawer on a tablet. The phone screens follow the saved mockups (`docs/design/android-ui.md`): big light titles, underlined
collection tabs, pill sub-tabs, a card mini player, a bottom bar with search and profile, a profile page with the
private status box, settings as cards, the film page, and the video player in portrait and landscape (the rotate
button turns one video; settings › video player sets how videos open). The accent is the design's yellow on phones.
Pick folders with the folder button; the Android picker's addresses are turned
into plain paths (`Paths::folderPath`), and the app asks for the media permissions on first start.

You need: JDK 17+, the Android SDK (platform 34, build-tools 34.0.0), NDK 26.1.10909125, Qt 6.8.3 for
`android_arm64_v8a` plus the same Qt for desktop (as `QT_HOST_PATH`), and TagLib and SQLite built for Android
(static, one prefix with `include/` and `lib/libtag.a`, `lib/libsqlite3.a`).

```sh
# TagLib 1.13.x and the SQLite amalgamation, cross-built into $PREFIX (see CMake's android.toolchain.cmake)
qt-cmake -S . -B build-android -G Ninja \
  -DQT_HOST_PATH=$QT_HOST -DANDROID_SDK_ROOT=$SDK -DANDROID_NDK_ROOT=$NDK \
  -DHOPTUNES_ANDROID_PREFIX=$PREFIX -DHOPTUNES_BUILD_TESTS=OFF
ninja -C build-android apk
```

The build defines no `HOPTUNES_HAVE_GST`: the GStreamer-only code (probing, thumbnails, the GStreamer video
player, MPRIS) compiles to small stubs, and `src/platform/android/gst/gst.h` stands in for GStreamer so `Player`
plays through Qt Multimedia (`src/platform/android/GstShim.cpp`). Desktop builds are unchanged.
