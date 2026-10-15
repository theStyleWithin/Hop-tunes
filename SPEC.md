# Hop-tunes: Product & Technical Spec

**Status:** Draft v0.4 (M1, M2 and M3 built) · **Date:** 2026-10-14 · **Platform:** Linux first

Hop-tunes is a simple, fast, local-first music player for Linux. Its organisation and feel are inspired by how the Zune desktop software *worked*: calm, typography-led, seamless. It has **no online accounts, no store and no device sync**. Several people can share one install, each with their own private local profile and library.

---

## 1. Goals and non-goals

### Goals
1. **A great music player first.** Gapless playback, fast library browsing, clean organisation.
2. **No login, ever.** Nothing in the app needs an internet account. The app works fully offline.
3. **Local profiles.** Multiple people on one computer each get their own profile with their own library view, playlists, play history, ratings and settings. Profiles are created inside the app and are independent of the operating system's user accounts.
4. **Zune-inspired UX, original implementation.** Reproduce the *behaviour* and *organisation* (pivots, collection layout, Now Playing, Quickplay) with original code, art and branding.
5. **Linux-native.** Feels at home on KDE and GNOME. Packaged as Flatpak and AppImage, with an AUR package for Arch/CachyOS.

### Non-goals (explicitly removed from the Zune concept)
| Removed | Why |
|---|---|
| Zune / Microsoft / Windows Live sign-in | The core design principle: no accounts |
| Marketplace, Zune Pass, rentals, purchases | The services are gone, and selling content is out of scope |
| Zune Social, friends, cards | Needs a server and accounts |
| Zune / KIN / Windows Phone device sync, MTPZ | Removed to stay simple and avoid MTPZ legal issues (may be revisited as a separate plugin later, see §11) |
| Telemetry or "phone home" of any kind | Privacy |
| Microsoft trademarks, logos, fonts, sounds, art | Legal: see §10 |

---

## 2. Users and scenarios

- **Solo listener:** opens the app and their library is just there. No setup beyond picking a music folder.
- **Shared family PC / Steam Deck:** three people use the same machine and OS account. Each picks their profile at launch and sees their own playlists, recently played, ratings and Quickplay.
- **Private listener:** wants their profile hidden behind a PIN so siblings can't browse their history.
- **Big-library collector:** 50k+ tracks on an external drive. Needs fast scanning, a drive that may be offline, and correct album-artist and compilation handling.

---

## 3. Local profiles (replaces Zune sign-in)

### 3.1 Concept
A **profile** is a local, offline container for one person's listening world. It is *not* an identity, *not* tied to an email address, and never leaves the machine.

### 3.2 Behaviour
- **First run:** the "Create your profile" screen asks for a name (required), a colour accent and an avatar picture (optional, picked from bundled original art or a local image), and the music folder(s) to watch.
- **Launch:**
  - With one profile, it opens straight into it. No picker.
  - With two or more, a profile picker shows big tiles (name, avatar, accent colour), plus "Add profile".
- **Optional PIN** per profile (4–8 digits). This is a **privacy curtain, not security**: it stops casual browsing, but anyone with access to the OS account can read the files. The UI says so plainly. An optional "encrypt this profile" mode is a later milestone (§12).
- **Switching** profiles from the top bar never needs a restart. Playback stops on switch.
- **Removing** a profile deletes only that profile's data (playlists, history, settings). It **never** deletes music files.
- **Guest mode:** a temporary profile that keeps no history and is wiped on exit.

### 3.3 What is per-profile vs shared
| Data | Scope | Notes |
|---|---|---|
| Watched folders | Per profile | Two profiles may watch the same folder |
| Track index (tags, durations, file paths) | **Shared cache**, keyed by file | Avoids scanning the same files twice |
| Artwork cache | Shared | Content-addressed by hash |
| Playlists, smart playlists | Per profile | |
| Play counts, last played, skips, ratings, "hearts" | Per profile | |
| Quickplay pins, history | Per profile | |
| Settings (theme, accent, EQ, crossfade, ReplayGain, layout) | Per profile | |
| Podcast subscriptions | Per profile | |

A profile only *sees* tracks that live under its own watched folders, even though the index is shared.

### 3.4 Storage layout (XDG)
```
$XDG_DATA_HOME/hop-tunes/            (~/.local/share/hop-tunes)
├── shared/
│   ├── index.sqlite                 track index: files, tags, audio properties
│   └── art/<sha256>.jpg             artwork cache
└── profiles/
    ├── profiles.json                list: id, name, accent, avatar, has_pin, created
    └── <profile-uuid>/
        ├── profile.sqlite           playlists, play stats, ratings, quickplay, podcasts (M4)
        ├── settings.json            watched folders, volume; more settings as they land
        └── avatar.png               256 px square, cropped from the picture the user picked
$XDG_CONFIG_HOME/hop-tunes/hop-tunes.conf   app-wide (Qt settings format)
$XDG_CACHE_HOME/hop-tunes/            cover art cache, thumbnails (safe to delete)
```
- PINs are stored in `profiles.json` as salted **PBKDF2-HMAC-SHA256** hashes
  (`pbkdf2-sha256$<iterations>$<salt>$<hash>`, 120 000 iterations), built from QtCore so the
  feature adds no dependency. A 4–8 digit PIN can be brute-forced by anyone who can read the
  file, whatever the hash, which is why the UI calls it a privacy curtain. After 5 wrong PINs
  the profile waits 30 s.
- `profiles.json` and `settings.json` are written atomically with `0600` permissions; profile
  folders are `0700`. Profile ids must be UUIDs, so a hand-edited file cannot point outside
  `profiles/`. An unreadable `profiles.json` is moved aside, never overwritten.
- **Upgrading from M1/M2:** the folders from the old app-wide setting are adopted by the first
  profile created, then the old setting is removed.
- Guest mode writes nothing to `profiles/`. It watches `~/Music`; files it scans still land
  in the shared index cache, which holds no personal data.

---

## 4. UX and organisation (Zune-inspired)

Hop-tunes takes the *organising ideas* that made Zune pleasant, not its assets:

### 4.1 Navigation model
- **Top pivots** in large lowercase type: **collection · now playing · quickplay · settings**.
- Collection has **sub-pivots**: **music · videos · pictures · podcasts**. *v1 ships music*; **videos** follows in v1.1 (§5b); pictures comes later.
- Inside music: **artists · albums · songs · genres · playlists**.
- Navigation is flat and text-led, with no toolbar clutter. Back/forward arrows sit top-left, and a persistent **mini player** sits along the bottom.

### 4.2 Collection: music
- The **three-pane browse** is the key Zune layout: *Artists* → *Albums by that artist* (art grid) → *Songs* on the selected album.
- Album view is a large art grid with title and artist under each tile. Sort by artist, title, year, date added.
- Song view is a dense list with columns # · title · artist · album · length · rating, sortable and configurable.
- An instant filter box searches as you type across artist, album and title.
- Multi-select, then right-click: play, play next, add to queue, add to playlist, edit tags, show in folder.

### 4.3 Now Playing
- Full-window view with a **mosaic of the artist's or library's album art** slowly drifting in the background, plus big type for title / artist / album.
- Up next: the queue is shown inline and can be reordered by drag.
- Minimal controls that fade out after inactivity.

### 4.4 Quickplay (start page)
- **Pinned:** user-pinned albums, artists and playlists.
- **History:** recently played.
- **New:** recently added to the library.
- **Smart DJ:** pick an artist, and Hop-tunes builds an endless mix from the *local* library using tag similarity (genre, year, co-occurrence in playlists, play history). No network.

### 4.5 Visual language (original)
- Typography-first: large, light-weight sans-serif with generous whitespace. Use an open font (Inter, Selawik-style alternatives, Noto Sans), **not** Segoe/Zune Pro.
- Dark and light themes, plus a user-chosen accent colour per profile.
- Motion: subtle slide-and-fade transitions between pivots (≤200 ms) and smooth kinetic scrolling. Respects the system "reduce motion" setting.
- Original icon set and app logo. No Zune shapes or colours.

### 4.6 Mini player and system integration
- An always-visible bottom bar: art thumbnail, title/artist, play/pause/next/prev, seek bar, volume.
- **MPRIS2** support, so media keys, the KDE/GNOME media widgets and the Steam Deck overlay all work.
- System tray icon (optional) and desktop notifications on track change (optional).

---

## 5. Playback

| Feature | v1 | Notes |
|---|:-:|---|
| Formats: MP3, AAC/M4A, FLAC, ALAC, Ogg Vorbis, Opus, WAV, WMA | ✓ | Via GStreamer plugins |
| Gapless playback | ✓ | Must be seamless on albums. This is the signature feel |
| Crossfade (0–12 s, off by default) | ✓ | Disabled automatically within a continuous album |
| ReplayGain (track/album), with tag fallback scanning | ✓ | |
| 10-band EQ + presets | v1.1 | Per profile |
| Shuffle, repeat one/all, queue, play next | ✓ | |
| Playback speed for podcasts | v1.2 | `scaletempo` |
| Resume position for long tracks / audiobooks (`.m4b`) | v1.1 | |

---

## 5b. Video (modern resolutions, including 4K)

Zune 4.8 (built August 2011, codename "Dorado") is a **32-bit** app with its **own bundled 2011-era decoders** (`ZuneH264Dec.dll`, `ZUNEMP4SDECD.dll`, `ZuneDXVA2.dll`, `ZuneEvr.dll`), and it ignores the system's codecs. That explains why it falls over on modern HD/4K files. It wasn't built for:
- **H.264 at 4K** (High profile, level 5.1/5.2), which is beyond what 2011 PC media players were tuned for;
- **HEVC/H.265, VP9 or AV1**, the codecs most 4K files actually use. Zune has no decoder for them at all;
- **10-bit / HDR** video;
- the memory that 4K frame buffers need, inside a 32-bit process.

*(This is an informed reading of the bundled components and their build dates, not a measured crash analysis.)*

Hop-tunes avoids all of this by using maintained, modern media engines instead of shipping its own decoders.

### Requirements
| Feature | Target |
|---|---|
| Resolutions | SD → 1080p → **1440p → 4K (3840×2160) → 8K** where the hardware allows |
| Codecs | H.264, **HEVC/H.265, VP9, AV1**, MPEG-4 Part 2, VC-1/WMV (legacy Zune-era files), MPEG-2 |
| Containers | MKV, MP4/M4V, WebM, MOV, AVI, WMV/ASF, TS |
| Bit depth / HDR | 8- and 10-bit; HDR10 / HLG with tone-mapping to SDR displays; HDR passthrough when the compositor supports it (KDE Plasma 6 HDR) |
| Frame rates | Up to 60 fps (120 fps where supported); frame-accurate seeking |
| **Hardware decoding** | **VA-API** (AMD / Intel, including Steam Deck), **NVDEC** (NVIDIA), **Vulkan Video** where available, with automatic, safe fallback to software decoding |
| Audio tracks | Multiple tracks, selectable; 5.1/7.1 downmix to stereo |
| Subtitles | SRT, ASS/SSA (styled, common for anime), PGS, embedded MKV subtitle tracks; per-profile default language |
| Resume | Per-profile resume position for each video |
| Chapters | MKV/MP4 chapters shown on the seek bar |

### Engine
- **Video playback uses `libmpv`**, embedded in the Qt/QML window through mpv's render API (OpenGL or Vulkan). mpv is the most battle-tested open-source video engine on Linux: it handles 4K/HDR, hardware decoding (`hwdec=auto-safe`), every subtitle format and odd files, and it is LGPL/GPL-licensed.
- **Music keeps the GStreamer engine** (§5), which is tuned for gapless, ReplayGain and library playback. Both engines sit behind one `PlaybackEngine` interface, so the UI (mini player, MPRIS, Now Playing) treats audio and video the same way.
- No bundled decoders. Codecs come from the system's FFmpeg/GStreamer, so the app gets new formats with OS updates (Flatpak: via the FFmpeg runtime extension).

### Performance targets
- **4K HEVC 10-bit at 60 fps** plays with **<15% CPU** on any GPU with a hardware decoder for it, such as the Steam Deck APU or AMD RDNA2+.
- Starting a 4K file takes **<1 s** to the first frame from local SSD storage.
- When the GPU can't decode a codec in hardware (for example AV1 on older GPUs), Hop-tunes falls back to software decoding and shows a small "software decoding" hint instead of crashing.

### Library side
- The **Videos** pivot lists items by type: movies, shows (series → season → episode, from folder names or filename patterns), music videos and other.
- Thumbnails come from a frame grab, cached in `$XDG_CACHE_HOME`.
- Video metadata is read with ffprobe-style probing at scan time (resolution, codec, HDR flag, duration). Badges like **4K · HDR · HEVC** appear on the tiles.
- Optional: point a Videos folder at the same library you already serve with Jellyfin. Hop-tunes only reads the files and never modifies Jellyfin's data.

---

## 6. Library

- **Sources:** one or more local folders per profile, including removable or network drives. Missing drives show tracks greyed-out rather than deleting them.
- **Scanning:** an initial scan in a background thread, then live updates through inotify. Target: index 10k tracks in under 60 s on an SSD.
- **Tags:** read via TagLib (ID3v1/v2, Vorbis comments, MP4 atoms, APE, ASF/WMA). Handles album artist, compilations, disc numbers, and sort tags (`ALBUMARTISTSORT`, …).
- **Artwork:** embedded art, then `cover.jpg`/`folder.jpg` in the folder, then a placeholder tile generated from album name + accent colour.
- **Tag editing:** single and bulk edit, written back to the files (with a confirmation dialog), or stored only in Hop-tunes (a per-profile override, so the files stay untouched).
- **Optional online metadata:** MusicBrainz / Cover Art Archive lookup. **Off by default**; switching it on is an explicit opt-in per profile, with no account needed.
- **Import:** `.m3u` / `.m3u8` / `.pls` / `.xspf` playlists, and `.zpl` (Zune/WMP playlist XML) for people migrating from Zune.

---

## 7. Playlists

- Manual playlists with drag and drop.
- **Smart playlists:** rule builder (genre is …, rating ≥ …, added in last N days, play count < …, etc.), live-updating.
- Auto lists: Most played, Recently added, Never played, Hearted.
- Export to `.m3u8` with relative paths.

---

## 8. Podcasts (v1.2, optional)
- Subscribe by RSS URL or OPML import. No account.
- Download or stream episodes, track played/unplayed state, auto-delete played episodes.
- Network access happens only for podcasts and only when the user adds a feed.

---

## 9. Architecture

### 9.1 Recommended stack
| Layer | Choice | Why |
|---|---|---|
| UI | **Qt 6 + QML** | Fluid animation and typography-heavy layouts are natural in QML. Native on KDE/CachyOS, good on GNOME, works on Steam Deck |
| Core | **C++20** (or Rust with cxx-qt, see open questions) | Performance for big libraries |
| Playback (music) | **GStreamer 1.x** (`playbin3`) | Gapless, ReplayGain, broad format support |
| Playback (video) | **libmpv** (render API in QML) | 4K/HDR, hardware decoding, every subtitle format |
| Tags | **TagLib 2** | Mature, all formats |
| Database | **SQLite** (WAL mode) + FTS5 for search | Fast, embedded, zero config |
| Desktop integration | MPRIS2 over D-Bus, XDG dirs, inotify | |
| Packaging | Flatpak (Flathub), AppImage, AUR | |
| Licence | **GPL-3.0-or-later** | Fits Qt (LGPL), GStreamer and TagLib; keeps forks open |

### 9.2 Modules
```
hop-tunes/
├── src/core/        profiles, settings, paths, PIN hashing
├── src/library/     scanner, watcher, tag reader, index DB, artwork cache, search
├── src/playback/    PlaybackEngine interface; GStreamer (music) + libmpv (video) backends; queue, gapless, ReplayGain
├── src/playlists/   manual + smart playlist engine, import/export (m3u, xspf, zpl)
├── src/smartdj/     local similarity mixer
├── src/mpris/       D-Bus MPRIS2 service
├── src/ui/qml/      pivots, views, mini player, theme
├── assets/          original icons, fonts (OFL), placeholder art
└── tests/
```

### 9.3 Core data model (simplified)
```
shared/index.sqlite
  track(id, path UNIQUE, mtime, size, duration_ms, codec, bitrate, sample_rate,
        title, artist, album_artist, album, disc, track_no, year, genre,
        compilation, rg_track_gain, rg_album_gain, art_hash)
  track_fts(title, artist, album, album_artist)        -- FTS5

profiles/<id>/profile.sqlite
  folder(path)
  stats(track_id, play_count, skip_count, last_played, rating, hearted)
  tag_override(track_id, field, value)
  playlist(id, name, kind{manual,smart}, rules_json, created)
  playlist_item(playlist_id, pos, track_id)
  quickplay_pin(kind{album,artist,playlist}, ref, pos)
  history(track_id, played_at)
```

---

## 10. Legal and clean-room rules

Hop-tunes is an **independent, original program**. It is inspired by how Zune behaved, and contains no Zune code or assets.

1. **Behaviour, not code.** Features and layouts are specified from *using* the Zune software and from public screenshots and reviews, and written down in plain language (this spec). Nobody copies, translates or paraphrases Microsoft code. Nothing from decompilers or disassemblers is used.
2. **No Microsoft assets.** No binaries, resources, images, sounds, fonts (Segoe, Zune Pro), UI markup or strings from Zune.
3. **No trademarks.** "Zune" appears only in docs as a factual reference ("inspired by Zune"). It is never in the app name, logo, UI or store listing, and the app never suggests Microsoft endorsement.
4. **Interoperability is fine.** Reading formats users already own (`.zpl` playlists, WMA files via GStreamer) uses public or widely documented formats.
5. **The teardown research** in `zune-teardown/` is reference documentation only. Hop-tunes code must not depend on it.
6. **Original visual design.** Inspired-by is fine; trace-overs are not. Draw icons and logo from scratch.

*This is a project guideline, not legal advice.*

---

## 11. Possible future (out of scope for v1)
- Pictures pivot.
- Optional "sync to a folder / generic MTP player / phone" through a plugin (plain MTP via libmtp, **no** MTPZ).
- Encrypted profiles (SQLCipher + key from PIN).
- Local network sharing (DLNA server) for TVs and consoles.
- Last.fm / ListenBrainz scrobbling as an *optional* plugin (that would be the only login, user-initiated and off by default).

---

## 12. Milestones

| Milestone | Scope | Done when |
|---|---|---|
| **M0: Skeleton** | Repo layout, CMake, Qt/QML window, CI (GitHub Actions), licence | Empty app builds and runs in CI |
| ✅ **M1: Play a folder** | Scanner + index DB + GStreamer gapless playback + song list | Can pick a folder and play an album gaplessly |
| ✅ **M2: Collection** | Artists/albums/songs pivots, art grid, cover art, mini player, MPRIS. *Search moved to M4.* | Daily-driver usable for one person |
| **M3: Profiles** ✅ | Profile create/pick/switch/delete, PIN, per-profile settings, guest mode (per-profile stats arrive with play history in M4) | Two people can share one install without seeing each other's data |
| **M4: Playlists & Quickplay** | Manual + smart playlists, import m3u/zpl, Quickplay pins/history/new, instant search | — |
| **M5: Now Playing & polish** | Art mosaic, transitions, themes/accents, ReplayGain, crossfade | Feels "seamless" |
| **M6: Ship 1.0** | Flatpak, AppImage, AUR, user docs | Published |
| **M7: Video (v1.1)** | libmpv engine, Videos pivot, 4K/HDR, VA-API/NVDEC hardware decoding, subtitles, resume | 4K HEVC 10-bit plays smoothly on Steam Deck |
| 1.1 / 1.2 | EQ, Smart DJ, tag editing write-back, podcasts | — |

---

## 13. Open questions
1. C++ or Rust (cxx-qt) for the core?
2. Should a PIN be required to *delete* another profile?
3. Should profiles be able to **share** a playlist (read-only link between profiles)?
4. Name and logo: keep "Hop-tunes"? Logo direction?
5. Steam Deck: is a controller-friendly "couch mode" layout worth putting into v1?
