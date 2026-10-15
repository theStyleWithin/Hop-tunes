# Changelog

All notable changes to Hop-tunes. Versions follow [Semantic Versioning](https://semver.org).

## [1.0.0] - 2026-10-15

First stable release.

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
