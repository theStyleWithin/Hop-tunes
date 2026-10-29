# Changelog

All notable changes to Hop-tunes. Versions follow [Semantic Versioning](https://semver.org).

## [1.5.0] - 2026-10-30

### Added
- **Hop-tunes for Windows 10 and 11 (64-bit), experimental:** an **installer** (`Hop-tunes-Setup-<version>-x64.exe`:
  per user by default, no administrator prompt, Start Menu and optional desktop shortcut, and
  uninstalling never touches your profiles or library) and a **single-file portable exe**
  (`Hop-tunes-<version>-portable-x64.exe`: no install, run it from anywhere, even a USB stick).
  The portable exe unpacks itself once on first launch (a few seconds, with a progress window) and
  then starts at once; your profiles, playlists and settings stay in a `Hop-tunes-data` folder
  beside it, so they travel with the file. Both include everything they need: Qt, GStreamer, libmpv.
- Both Windows builds are built and tested on GitHub's Windows machines before they are published
  (the full test suite; the app started from the staged folder, from a real silent install and
  uninstall, and from the portable exe; and a generated song found and played).

### Known limits on Windows
- The files are **not code-signed**, so Windows SmartScreen may warn on first run: choose
  "More info", then "Run anyway".
- No Windows media keys or system media overlay yet, and opening a second file does not hand it to an
  already-running window.
- **WARNING: the developer never tests on Windows.** Hop-tunes is tested on Linux (and Android, once it
  exists), not Windows. These builds have only ever been run by automated tests on GitHub's Windows
  machines, never by a person on a real PC. **Install at your own risk. Windows testers wanted:** please
  report what works and what doesn't at https://github.com/theStyleWithin/Hop-tunes/issues.

### Fixed
- A damaged profile database could not be replaced on Windows (the file was still open when the app
  tried to move it aside); it is now closed first.

### Changed
- Tests no longer share the real user folders (they use `HOPTUNES_DATA_DIR` and
  `HOPTUNES_MUSIC_DIR`, new overrides that also work for kiosks and portable setups), and a flaky UI
  test now waits for its popup instead of failing on slow machines.

## [1.4.0] - 2026-10-29

### Added
- **Four new collection sections: pictures, audiobooks, ebooks and podcasts.** Each has its
  own per-profile folder list and its own "choose a folder" button (also listed in settings,
  one list per section); nothing is scanned until you add a folder to that section.
  - **pictures**: a thumbnail grid and a full-window viewer (arrow keys, slideshow).
  - **audiobooks**: plays through the normal player and remembers where you stopped, also
    after closing the app; the list shows how far you are.
  - **podcasts**: local episode files grouped by show, with the same resume, plus played /
    unplayed marks. Local files only: no feeds, no network.
  - **ebooks**: EPUBs open in a built-in reader (chapters, contents list, text size, saved
    reading position). PDFs open in your default PDF viewer.
- **Videos have their own folders.** A "choose a video folder…" button in the empty videos
  tab and a "video folders" list in settings. Until you add one, videos keep being found under
  your music folders, so existing libraries are unchanged; the first video folder you add starts
  from those folders so nothing disappears.
- **Pick your own video thumbnail.** An "image" button under each video opens a picture picker
  (PNG/JPEG), with a "reset" link back to the detected frame. Saved per profile, hidden in
  guest mode.

### Changed
- The settings folder list that feeds music is now labelled "music folders".
- Profile databases migrate to schema v6 automatically on first launch (new tables only; no
  data is changed or removed). EPUB reading adds a build dependency on zlib.

## [1.3.1] - 2026-10-17

### Fixed
- **Video still not playing after v1.2.0's locale fix** (found on the same Steam Deck
  running CachyOS Handheld Edition): the `mpv_create failed` error was gone, but the player
  screen came up black/frozen at 0:00 with no picture or audio and no visible error.
  `MpvVideoItem::setResumePosition()` was missing `Q_INVOKABLE`, so QML's call to it — the
  very first thing that happens on every video open — threw and silently aborted before the
  video ever loaded. Fixed and **confirmed on the user's actual Steam Deck** — video plays.
- **Video rendered upside down**, found from that same hardware test: `MPV_RENDER_PARAM_FLIP_Y`
  was `1`; mpv's own Qt Quick render-API example uses `0` for this FBO integration. **Also
  confirmed fixed on the user's actual Steam Deck.**

## [1.3.0] - 2026-10-16

### Added
- **M6 quality of life: per-profile album overrides, and live drag-reorder everywhere.**
  An album's cover and title can now be edited right in the Collection grid — an "image"
  button under the art opens a picture picker, and the title is a clickable, editable
  field — and its tracks dragged into a custom order live, even mid-playback. The *songs*
  tab (the whole library) now drags and reorders the same way, not just albums and manual
  playlists. All of it is per-profile and private (`SPEC.md` §3.3): none of it is ever
  visible to another profile sharing the same install, and guest mode keeps the live
  reorder for the session but saves nothing.
- Quickplay's *pinned* strip now groups into labelled artists/albums/playlists sub-rows
  instead of one mixed row, so a few pins of different kinds don't read as clutter.

## [1.2.0] - 2026-10-16

### Fixed
- **Video playback failing with "mpv_create failed" on real hardware** (found testing
  v1.1.0 on a Steam Deck running CachyOS Handheld Edition). libmpv's own sanity check in
  `mpv_create()` rejects a process whose `LC_NUMERIC` locale category isn't `"C"` — it
  parses its own option values assuming `.` as the decimal separator — and Qt/GLib/
  GStreamer startup code commonly sets the process locale from the desktop environment
  before any video playback code runs. CI's runners default to the `C` locale already,
  which is why this was never caught before a real desktop exercised it. `MpvCore` now
  reasserts `LC_NUMERIC=C` right before every `mpv_create()` call, with a regression test
  that reproduces the exact failure under a non-`C` locale.

### Changed
- Settings' folder list, and its description text, are now called "media folders" instead
  of "music folders" — the same folder list has fed the video scanner since M7, so the
  old label was inaccurate.

## [1.1.0] - 2026-10-16

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

[1.3.1]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.3.1
[1.3.0]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.3.0
[1.2.0]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.2.0
[1.1.0]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.1.0
[1.0.0]: https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.0.0
[0.1.0]: https://github.com/theStyleWithin/Hop-tunes/commits/main
