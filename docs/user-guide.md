# Hop-tunes user guide

Hop-tunes plays the music files you already own. It is fully offline: no accounts, no
sign-in, no store and no tracking. It never changes, moves or uploads your files.

- [Install](#install)
- [First run: your profile](#first-run-your-profile)
- [Your music](#your-music)
- [Playing music](#playing-music)
- [Playlists](#playlists)
- [Quickplay and smart DJ](#quickplay-and-smart-dj)
- [Profiles](#profiles)
- [Settings](#settings)
- [Keyboard and media keys](#keyboard-and-media-keys)
- [Where Hop-tunes keeps its data](#where-hop-tunes-keeps-its-data)
- [Troubleshooting](#troubleshooting)
- [Uninstalling](#uninstalling)

## Install

Pick one. All three are the same app.

| | Best for | How |
|---|---|---|
| **Flatpak** | Most desktops; updates through your software centre | Download `Hop-tunes-x86_64.flatpak` from the [releases page](https://github.com/theStyleWithin/Hop-tunes/releases), then `flatpak install --user Hop-tunes-x86_64.flatpak`. You need the Flathub remote (most distros have it). |
| **AUR** | Arch, CachyOS, EndeavourOS, Manjaro | `yay -S hop-tunes` (or `paru -S hop-tunes`). `hop-tunes-git` builds the latest code. |
| **AppImage** | Anything else; no install needed | Download `Hop-tunes-…-x86_64.AppImage`, make it executable (`chmod +x`, or *Properties → Allow executing*), and run it. Needs a 2022-or-newer x86_64 distro (glibc 2.35). |

The full list of tested distros (CachyOS first), and what to use on older ones, is in
[COMPATIBILITY.md](COMPATIBILITY.md).

## First run: your profile

The first time you open Hop-tunes it asks for:

- **a name** (only used on this computer),
- **a colour** (the accent used across the app),
- **your music folder** (it suggests your *Music* folder),
- optionally a **PIN** of 4 to 8 digits.

> A PIN is a *privacy curtain*, not a lock. It stops casual browsing, but anyone who can use
> your computer account can still read Hop-tunes' files.

Or choose **just listen as guest**: guest mode plays your Music folder and saves nothing.

## Your music

Hop-tunes reads these formats: MP3, FLAC, AAC/M4A, ALAC, Ogg Vorbis, Opus, WAV, AIFF, WavPack,
APE, Musepack and WMA. It scans in the background and only re-reads files that changed.

- **Add or remove folders** in *settings → media folders*. Removing a folder only stops
  Hop-tunes looking there; your files stay where they are.
- **Cover art** comes from the art embedded in your files, or an image called `cover`,
  `folder`, `front` or `album` (`.jpg` or `.png`) next to them.
- **Browse** in *collection → music*: **artists** (three panes: artist → albums → songs),
  **albums** (a grid of covers), **songs** (everything in one list), and **playlists**.
- **Search**: type in the search box (or press `Ctrl+F` or `/`). Every word must match the
  title, artist, album or genre; case and accents don't matter, so "beyonce" finds "Beyoncé".
  `Esc` clears it.

## Your videos

Hop-tunes also plays video — `collection → videos` lists anything it finds under your
media folders (`.mkv`, `.mp4`, `.m4v`, `.webm`, `.mov`, `.avi`, `.wmv`, `.asf`, `.ts`,
`.m2ts`). Click a tile to play it full-window.

- **Badges** on each tile show resolution (SD/720p/1080p/1440p/4K), codec and HDR, from a
  quick probe at scan time.
- **Hardware decoding** is used automatically when available, with a safe fallback to
  software decoding — nothing to configure.
- **Subtitles** load automatically: embedded tracks, or a same-named `.srt`/`.ass` file
  next to the video (`movie.mkv` + `movie.srt`). The "CC" button in the player cycles
  through the tracks it found, including off.
- **Resume**: closing a video partway through picks up where you left off next time,
  unless you were close enough to the end that it counts as finished.
- Video plays through `libmpv` if it's installed (see **Building**, or your distro
  package for `libmpv`/`mpv`), with better subtitle styling and hardware decode. Without
  it — as in the Flatpak build today — Hop-tunes falls back to GStreamer's own Qt video
  sink instead, which needs no extra install beyond what Hop-tunes already uses for
  music. Either way the Videos grid works the same; if truly neither is available,
  opening a video shows a plain message instead of a crash.

## Movie pages

Click a movie in `collection → videos` to open its page; **nothing plays until you press play** (or "resume"). The page is a set of empty boxes for you to fill in (title, year, rating, score, tagline, description, genre, director, writers, studio, tags, and cast & crew with an optional photo each) plus "change poster" and "mark watched". Hop-tunes never looks anything up online; the only things it fills in itself are facts from the file (length, resolution, codec, size). It is saved in your profile.

## Watching: window, theater, fullscreen

Pick how video opens in **settings → video settings → viewing style**. **Window** (the default) shows the video with a list of your other videos beside it and the movie's details underneath; **theater** stretches the video across the window with the details below; **fullscreen** fills the screen. While watching, the "theater" and "fullscreen" buttons (or `t` and `f`) switch for the moment, `esc` steps back out of fullscreen, and `space` pauses. Clicking another video in the list switches to it and remembers where you were in the first.

## Videos have their own folders

`collection → videos` can have its own folders, separate from your music. Until you add one,
videos are found under your music folders, so nothing changes for an existing library. The
first video folder you add starts from those folders (so nothing disappears); remove the ones
you don't want in **settings → video folders**. The "image" button under a video lets you pick
your own thumbnail (and "reset" returns to the detected frame); it is saved per profile.

## Pictures, audiobooks, ebooks and podcasts

Four more `collection` tabs, each with **its own folders and its own "choose a folder" button**
(also listed in **settings**, one list per section). Nothing is scanned until you add a folder
to that section, and a folder in one section is never read for another.

- **pictures**: a thumbnail grid (jpg, png, webp, gif and a few others). Click one to open the
  full-window viewer: left/right arrows (or the buttons) move, space starts or stops a
  slideshow, escape closes.
- **audiobooks**: m4b, mp3, m4a, ogg, opus, flac. Click to play. Hop-tunes remembers where you
  stopped (also after closing the app) and picks up there; the list shows how far you are.
- **ebooks**: epub opens in the built-in reader (chapters, a contents list, text size, left/right
  arrows for chapters, space and the page keys to scroll). Your place is remembered. pdf files
  open in your default PDF viewer.
- **podcasts**: local audio episodes, grouped by show (the album tag, or the folder name).
  Resume works like audiobooks, and finished episodes are shown dimmed with "played"; "mark
  played" toggles it by hand.

Everything here is private to the profile and works offline. Hop-tunes only reads these folders.

## Playing music

- **Double-click** a song (or press `Enter`) to play from there. **Play album** plays the
  selected album.
- **Right-click** a song or album for: *play next*, *add to queue*, *add to playlist*,
  *heart*, and (for albums and artists) *pin to quickplay*.
- **The mini player** along the bottom has shuffle, previous, play/pause, next, repeat
  (off → all → this song), the seek bar and volume. Click the song name to open
  **now playing**.
- **Now playing** shows big art and the **up next** list. Drag a song by its `≡` handle to
  move it, click it to jump there, or use `×` to take it out. The controls fade away after a
  few seconds; move the mouse to bring them back.
- **Hearts** (♡) mark favourites. They collect in the *hearted* list and guide the smart DJ.
- **Opening files**: open music or video files, or folders, with Hop-tunes from your file
  manager, or run `hop-tunes song.flac movie.mkv some-folder/`. They start playing even if
  they're not in your media folders (a video opens the player directly). If Hop-tunes is
  already open, they go to that window.

## Playlists

In *collection → music → playlists*:

- **AUTO** lists build themselves: *hearted*, *most played*, *recently added* (last 30 days)
  and *never played*.
- **+ new playlist** makes an empty playlist. Add songs by right-clicking them (or an album)
  and choosing *add to playlist*. Reorder by dragging the `≡` handle, or use *move up / down*
  in the right-click menu.
- **+ smart playlist** builds a list from rules, such as *genre contains jazz* and
  *added in the last 30 days*, or *play count is more than 10*. Choose "all" or "any" of the
  rules, an order and an optional limit; the editor shows how many songs match as you go. Smart
  playlists update by themselves.
- **import…** reads `.m3u`, `.m3u8`, `.pls`, `.xspf`, and Zune or Windows Media `.zpl`/`.wpl`
  files. Playlists made on another computer (even a Windows Zune library) are matched to
  your songs by artist, album and file name. Hop-tunes tells you if any songs weren't found.
- **⋯ → export as .m3u8** saves a playlist other players can open. Paths are relative, so
  the playlist keeps working if you move your music and the playlist together.

## Quickplay and smart DJ

*quickplay* is a start page:

- **smart dj**: pick an artist and press *start mix*. Hop-tunes keeps playing music like it
  (the same artist, then similar genres and years, nudged by your hearts and what you play
  most) from your own library. It never goes online. Playing anything else ends the mix.
- **pinned**: albums, artists and playlists you pinned (right-click → *pin to quickplay*).
- **history**: albums you played recently.
- **new**: albums most recently added to your music folders.

Click a tile to play it; right-click for more.

## Profiles

Everyone who shares the computer can have their own profile: their own music folders,
playlists, hearts, play counts, history and settings. Your profile picture is in the top
right; click it to **switch profile** (playback stops, no restart needed).

In *settings → profile* you can rename the profile, change its colour or picture, set,
change or remove the PIN, or **remove the profile**. Removing a profile deletes only its
Hop-tunes data (playlists, history, settings); it never touches music files.

## Settings

- **playback → crossfade**: 0 (off, gapless) to 12 seconds. Songs from the same album
  always play gapless, without a fade.
- **playback → even volume**: uses the ReplayGain tags in your files so songs play at a
  similar loudness. *Per song* evens everything out; *per album* keeps an album's own quiet
  and loud moments. The pre-amp (−15 to +15 dB) shifts the overall level. Songs without
  ReplayGain tags get a small −6 dB cut so they sit near tagged ones. Changes apply from the
  next song.
- **appearance → theme**: dark, light, or match your desktop (with Qt 6.5 or newer).
- **appearance → motion**: *reduce motion* turns off the sliding transitions.

All settings belong to the profile you're using.

## Keyboard and media keys

| Key | Action |
|---|---|
| `Space` | play / pause |
| `Ctrl+→` / `Ctrl+←` | next / previous song |
| `Ctrl+F` or `/` | search |
| `Esc` | clear the search |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | collection / now playing / quickplay |
| `Ctrl+,` | settings |
| Media keys | play, pause, next, previous |

Hop-tunes also shows up in your desktop's media controls (KDE, GNOME, the Steam Deck
overlay, `playerctl`), which can play, pause, skip, seek, change the volume, and switch
shuffle and repeat.

## Where Hop-tunes keeps its data

| What | Where (AppImage / AUR) |
|---|---|
| Track index (shared by all profiles; rebuilt if deleted) | `~/.local/share/hop-tunes/shared/index.sqlite` |
| Profiles (settings, playlists, play counts, history) | `~/.local/share/hop-tunes/profiles/` |
| Cover art cache (safe to delete) | `~/.cache/hop-tunes/` |

The Flatpak keeps the same folders under `~/.var/app/io.github.thestylewithin.Hop-tunes/`.

## Troubleshooting

**No sound.** Hop-tunes plays through PipeWire or PulseAudio like other apps, and falls back
to ALSA. Check that Hop-tunes is not muted in your desktop's volume mixer, and that the
volume slider in the mini player isn't at zero.

**Some files don't play (AAC, WMA).**
- **AUR:** install `gst-libav` (and `gst-plugins-ugly` for some MP3/WMA files).
- **Flatpak:** the codecs come from the runtime's `codecs-extra` extension, which Flatpak
  installs with the runtime. If AAC or WMA files still won't play, run `flatpak update`.

**Songs are missing.** Check *settings → media folders* and press *rescan now*. Hop-tunes
skips files it can't recognise as audio. In the Flatpak, folders outside your Music folder
must be added with *+ add folder*, which grants access to them.

**I forgot my profile PIN.** Because a PIN is only a curtain, you can remove it. Close
Hop-tunes and run `hop-tunes --forget-pin "Your Name"` (Flatpak:
`flatpak run io.github.thestylewithin.Hop-tunes --forget-pin "Your Name"`). Your playlists,
history and settings are not affected.

**The window doesn't open on a very old distro (AppImage).** The AppImage needs glibc 2.35
or newer (Ubuntu 22.04, Debian 12, Fedora 36 or newer). Use the Flatpak instead.

**Rare freeze when skipping.** If skipping ever freezes playback, start Hop-tunes with
`HOPTUNES_PLAYBIN=playbin hop-tunes` to use GStreamer's classic engine, and please open an
issue.

Found a bug? Please open an issue: https://github.com/theStyleWithin/Hop-tunes/issues

## Uninstalling

- **Flatpak:** `flatpak uninstall io.github.thestylewithin.Hop-tunes` (add `--delete-data` to
  remove your profiles too).
- **AUR:** `sudo pacman -R hop-tunes`.
- **AppImage:** delete the file.

To remove your profiles and settings as well, delete the folders listed in
[Where Hop-tunes keeps its data](#where-hop-tunes-keeps-its-data). Your music files are never
touched.
