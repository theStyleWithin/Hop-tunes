# Changelog

All notable changes to Hop-tunes. Versions follow [Semantic Versioning](https://semver.org).

## [Unreleased]

### Added
- **M7: video playback.** A `collection › videos` grid (scanning, GStreamer-probed codec/
  resolution/duration, frame-grab thumbnails, 4K/HDR/codec badges) and a video player with
  two engines: **libmpv** (richer subtitles, `hwdec=auto-safe`) where available, falling
  back to **GStreamer**'s own Qt video sink (`qml6glsink`) where it isn't — currently the
  Flatpak build, with no extra dependency beyond what Hop-tunes already uses for music.
  Both engines auto-load subtitles (embedded tracks and same-name `.srt`/`.ass` sidecar
  files) and remember a resume position. See [`SPEC.md`
  §5b](SPEC.md#5b-video-modern-resolutions-including-4k) for exactly what's verified —
  notably, the GStreamer path's on-screen rendering was checked end to end (a real video,
  visibly playing); the libmpv path's render code follows mpv's documented API but
  couldn't be checked against real GPU hardware or a real display.
- **Live library updates.** Folders are watched (inotify via `QFileSystemWatcher`,
  debounced ~1.5 s), so a song or video dropped into a watched folder appears without a
  manual rescan. Purely local — no network involved.

### Removed
- The planned optional MusicBrainz/Cover Art Archive lookup (`SPEC.md` §6) was dropped
  before being built: it doesn't fit an offline-only player, opt-in or not.
- The planned podcasts feature (RSS/OPML subscriptions, `SPEC.md` §8) is dropped for the
  same reason, before being built: it was the only thing in the spec that needed network
  access at all, and that doesn't fit an always-offline player. Collection's sub-pivots
  are now just music and videos.

## [1.0.0] - 2026-10-15

First release, feature complete.

> **Download at your own risk.** Hop-tunes 1.0 passes its automated test suite on 16 Linux
> distributions, but it has not yet been tested by the developer on real hardware. If you
> choose to try it, you do so at your own risk. Running it in a virtual machine is recommended.
> See [which distros it runs on](https://github.com/theStyleWithin/Hop-tunes/blob/main/docs/COMPATIBILITY.md).

### Added
- **Local profiles** instead of accounts: a "who's listening?" picker, optional PIN (a privacy
  curtain, not security), colour and picture per profile, guest mode that saves nothing.
- **Playlists**: manual playlists (drag to reorder), smart playlists with a rule builder, and
  built-in lists: hearted, most played, recently added, never played.
- **Import and export**: `.m3u`, `.m3u8`, `.pls`, `.xspf` and Zune `.zpl`/`.wpl` playlists
  (even ones made on Windows); export as `.m3u8` with relative paths.
- **Quickplay**: pinned albums, artists and playlists, recently played, new albums, and an
  offline smart DJ.
- **Instant search** across titles, artists, albums and genres (case- and accent-insensitive).
- **Hearts and play counts** per profile.
- **Playback**: crossfade (0–12 s; albums stay gapless), even volume with ReplayGain (per song
  or per album), shuffle, repeat all or one, play next, add to queue.
- **Now Playing**: an up-next list you can reorder, controls that fade while you listen.
- **Themes**: dark, light, or match the system; smooth transitions with a "reduce motion" option.
- **Open files** from a file manager or the command line; a second launch reuses the open window.
- `--help` and `--version`, a man page, and an About section with keyboard shortcuts.
- **Packages**: Flatpak, AUR (`hop-tunes`, `hop-tunes-git`) and the portable AppImage.

- **Release kits** for AUR and Flathub publishers, verified by real `makepkg`/`namcap` and
  `flatpak-builder`/`flatpak-builder-lint` before every release.

### Fixed
- Building against Qt 6.4 (Ubuntu 24.04, Debian system Qt).
- A damaged track index no longer stops Hop-tunes from starting (it is set aside and
  rebuilt); a damaged profile database is set aside and started afresh.
- Junk entries in damaged or binary playlist files are ignored.

## [0.1.0] - 2026-10-14

First preview: library scanning, gapless playback, artists/albums/songs, cover art, Now
Playing, media keys (MPRIS) and the portable AppImage.

[1.0.0]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.0.0
[0.1.0]: https://github.com/theStyleWithin/Hop-tunes/commits/main
