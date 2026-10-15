# Hop-tunes AppImage

One file that runs on most Linux desktops without installing anything.

## Run it

```sh
chmod +x Hop-tunes-*-x86_64.AppImage
./Hop-tunes-*-x86_64.AppImage
```

Your music library and settings live outside the AppImage (`~/.local/share/hop-tunes/`,
`~/.config/hop-tunes/`), so replacing or deleting the file never touches your data.

If your system has no FUSE (some minimal installs), run it with
`./Hop-tunes-*.AppImage --appimage-extract-and-run`.

## Compatibility

Built on Ubuntu 22.04, so it needs **glibc 2.35 or newer** (any x86_64 desktop distro from
2022 onward). Every build is automatically tested in clean containers:

| System | Result |
|---|---|
| Ubuntu 22.04 LTS, 24.04 LTS | ✅ tested every build |
| Debian 12, Debian 13 | ✅ tested every build |
| Linux Mint 22 | ✅ tested every build |
| Fedora 41 | ✅ tested every build |
| Arch Linux (and Arch-based: CachyOS, EndeavourOS, Manjaro, SteamOS 3) | ✅ tested every build (Arch) |
| openSUSE Tumbleweed | tested every build |
| Real X11 session (Xvfb) | ✅ tested every build |
| Real Wayland session (Weston) | ✅ tested every build |
| AlmaLinux / RHEL 9 (glibc 2.34) | ❌ below the floor: the app starts, but some codecs need glibc 2.35 |

Not supported: 32-bit or ARM systems, and musl-based distros (Alpine, Void-musl). These need
separate builds.

Each test launches the AppImage, scans a music folder with the bundled TagLib, **plays a
song through the bundled GStreamer**, renders the UI and checks the log for errors.

## What's inside, and what comes from your system

Bundled (so your distro's versions can't break it):
- Qt 6.8 LTS (Quick, Controls, Dialogs) with X11, Wayland and offscreen backends, plus
  Wayland shell integration
- GStreamer with only the ~30 audio plugins Hop-tunes uses: MP3, FLAC, AAC/M4A, ALAC,
  Ogg Vorbis, Opus, WAV, AIFF, WavPack, WMA, Matroska audio; PulseAudio/PipeWire and ALSA output
- TagLib, SQLite, the Jost font

From your system, as every AppImage expects: glibc, your graphics driver
(OpenGL/EGL/Vulkan), X11/Wayland client libraries, fontconfig and ALSA.

A few small libraries that full desktops always have but minimal installs sometimes lack
(`libOpenGL`, `libGLdispatch`, `libcom_err`, `libgpg-error`, `libfribidi`) are shipped as
**fallbacks**. They're used only when your system doesn't have its own copy
(`apprun-hooks/hop-tunes-fallback-libs.sh`).

## Built to last

- **No online dependencies.** Hop-tunes never needs a server, an account or a download to
  keep working. A copy you keep today will still run years from now on any system that
  meets the glibc floor.
- **Long-term-support foundations.** Qt 6.8 is an LTS release; GStreamer, TagLib and SQLite
  keep stable APIs and file formats over decades.
- **Your data is versioned.** The library database carries a schema version and upgrades
  itself in place (`TrackIndex::migrate`), so newer builds read older libraries.
- **Updates are optional, never forced.** The AppImage carries update information, so tools
  like AppImageUpdate or Gear Lever can fetch a newer build (delta download) if *you* choose.
  Nothing updates automatically.

## Building it yourself

On Ubuntu 22.04 (or older, for an even lower floor), with Qt 6.8 installed:

```sh
packaging/appimage/build-appimage.sh
```

CI does the same in `.github/workflows/appimage.yml` and publishes the result to the
[`continuous` pre-release](https://github.com/theStyleWithin/Hop-tunes/releases/tag/continuous).
