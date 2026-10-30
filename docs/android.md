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

## Keeping the desktop and phone UIs apart

The phone layout and the desktop layout are two designs that share one program. To keep them from mixing:

- **Phone-only screens live in their own files:** `Phone*.qml` and `MobileNav.qml`. The desktop never loads them.
- **A screen both share switches on `Theme.compact`**, which is true only for a phone-sized window (the shorter side is under 600 dp). The phone version is a separate branch of that screen; the desktop branch keeps its sizes. `Theme.fs(px)` is the identity on desktop, so type sizes there do not change.
- **Tablets (600 dp and wider on the short side) use the desktop layouts**, plus the side rail or permanent drawer.
- **A change for one side never changes the other.** A new feature touches the shared code first and then gets its phone version in the matching `Phone*` file or `Theme.compact` branch. A pull request says which side it touches: desktop, phone or both.
- **Checks that guard it:** the desktop UI tests must all pass, and `phoneScreensFitTheScreen` fails when anything runs past the edge of a phone screen (412 and 320 dp wide). Anything you can see on a phone gets a render next to the saved mockups (`docs/design/android-ui.md`) before it counts as done.
