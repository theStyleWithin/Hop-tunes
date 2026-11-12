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
- **Keep your music where Hop-tunes can find it, and check carefully that it is the right folder.** If a music
  folder lives on an external drive, a network share or a USB stick, plug it in (and let it mount) before you
  press play. Songs whose files are not there cannot play: Hop-tunes skips each missing song, so with the drive
  unplugged it can look as if pressing play does nothing and the list jumps to the bottom. Also look in
  *settings → media folders* that the folder you actually use is the one that is added (it is easy to have a
  second copy of the same music in another place, one that is connected and one that is not).
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

## Livestream chat (a pretend live chat)

Turn on **settings → video settings → livestream chat** for a chat beside the video, as if you were watching a stream. **Nothing is streamed and nobody in it is real**: the chatters are made up on your computer, the chat is the same every time you watch the same video at the same moment (so seeking just redraws it), and nothing is saved. They react to the details you filled in on the movie's page, and you can type to them: they answer, politely, using what the page says (ask who directed it and they will tell you if you filled it in). Choose the **clean** or **lively** look in the same place. While watching, `c` or the "chat" button shows and hides it; press `esc` once to leave the chat box, again to step back.


## Picture albums

**collection › pictures** is a shelf of albums: one slot for every folder that has pictures in it, showing a cover, the name and the number of pictures. Click a slot to open it and see its pictures (click one for the full-size viewer: arrow keys to move, space for a slideshow, esc to close); **‹ albums** goes back. Each folder you add stays its own album, and so does each subfolder that holds pictures. Hover a slot (or open the album) and choose **rename** to give it your own name; it is remembered per profile. Clearing the name goes back to the folder's name.

## Movie volume

The video player has its own **volume slider** (next to the play button), separate from the music volume and from your computer's volume. It goes from 0 to **150 %**: the orange part past the 100 % mark boosts films that are recorded too quietly (very loud boosts can distort, so turn it up only as far as you need). The button beside it mutes. Keys: **↑ / ↓** change the volume, **m** mutes. It is remembered per profile.

## Video categories

The **videos** tab has five sub sections: **movies, shows, cartoons** (western animation), **anime** and **other videos** (for example videos you saved from YouTube to keep). Each has its own folders: use *choose a folder* (or *+ add folder*) inside the section, or the matching list under **settings**. Your existing video folders are your *movies*. A video belongs to the category whose folder is closest to it, so you can put an *Anime* folder inside a bigger movies folder and add it under anime. Everything still works the same in every category: movie pages, thumbnails, resume, chat and comments.

## Comments under videos

**settings → video settings → comments** has three choices, and it only shows in the *window* and *theater* styles (never fullscreen):

- **off** (default): no comments section at all. Choose this if you see it as clutter.
- **only mine**: a box under the video's details where you write comments, as many as you like. Nobody ever replies. Your comments are saved with the video in your profile, and each has a *delete* link.
- **populated**: the same box, plus made-up commenters with likes and replies (*show N replies*), sorted by *top* or *newest first*. *Show more comments* never runs out. A few of them answer what you write after a little while (a question about the director gets the director you filled in, or an honest "not sure").

Nobody here is real. The comments are generated on your computer from the details you filled in on the movie's page, and are never stored; only the ones you write are.

## Livestream chat (a pretend live chat)

Turn on **settings → video settings → livestream chat** for a chat beside the video, as if you were watching a stream. **Nothing is streamed and nobody in it is real**: the chatters are made up on your computer, the chat is the same every time you watch the same video at the same moment (so seeking just redraws it), and nothing is saved. They react to the details you filled in on the movie's page, and you can type to them: they answer, politely, using what the page says (ask who directed it and they will tell you if you filled it in). Choose the **clean** or **lively** look in the same place. While watching, `c` or the "chat" button shows and hides it; press `esc` once to leave the chat box, again to step back.

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

## Apps (off until you switch them on)

Apps are small programs that are part of Hop-tunes and open inside its window. They are **off** until you turn them on: go to
**settings → apps settings** and choose **show the apps tab**. A fresh Hop-tunes shows every other tab and stays exactly as quiet as before.

### Kick

Kick is the first app. It browses the live streams on kick.com in a wide menu and plays the one you pick, inside Hop-tunes. Nothing plays
until you click a stream, and no Kick account is needed.

On an Android phone the app is the same (apps are switched on in **settings → apps settings** there too): one column of streams, the picture on top
of the watch page and the chat below it. The Android app carries the internet permission for this; the video plays through Android's own player.

- **Allowing it.** The first time you open Kick it asks, and lists exactly which sites it will contact (kick.com, web.kick.com,
  images.kick.com, stream.kick.com, files.kick.com, clips.kick.com and live-video.net) and what is sent (what you ask for and your internet address, like any visitor).
  You can take the permission back any time in **settings → apps settings → kick**. It uses the internet only while the app is open, and the bar at
  the bottom shows what is connected, with a **disconnect** button. Nothing is kept on disk and no cookies are stored.
- **Browsing.** Search, pick a category, sort by most viewers or featured, choose a language. Adult (18+) streams are hidden unless you tick
  **show 18+**. **show more** and **refresh** are buttons: Hop-tunes never loads more in the background.
- **The developer's stream.** When the developer of Hop-tunes (gamehopper) is live, their stream takes the first card of the default feed, marked
  **developer**, ahead of Kick's own order. It is one extra lookup of that channel when the list loads, it never plays by itself, and it is
  not added to searches, categories or language filters. Your own choices win: if you block that channel, or keep 18+ streams hidden and that stream is
  marked 18+, you will not see it.
- **Following and blocking stay on this computer.** The **follow** button on a stream's page (or in a card's ⋯ menu) adds the channel to a
  list in your profile; it is shown as a "following" row with live dots, checked only when you open the app or press refresh. **Block** hides
  a streamer for good from every list and search; unblock them in **settings → apps settings**. Hop-tunes never follows, subscribes or
  does anything on your behalf on Kick.
- **Chat.** Beside the stream you choose **none** (the start) or **pretend**: a made-up chat where nobody is real (it says so). Kick's own
  live chat is planned for people who choose to sign in to Kick; signing in is optional and never needed to watch.
- **Good to know.** Kick's website addresses are not an official, promised interface and can change; if something stops working, Kick says
  why in plain words. Pictures are WebP: on Linux they need Qt's image-format plugins (`qt6-imageformats` on Arch, `qt6-image-formats-plugins`
  on Debian and Ubuntu; the Flatpak, AppImage and Windows builds include them). Playing the stream uses libmpv (AppImage, Windows, AUR) or, in the Flatpak, GStreamer, which needs its HLS plugin: if
  a stream does not play there, the message under the picture says so.

On a phone, the **search** button in the bottom bar searches Kick while the Kick app is open (type a streamer's name and press enter, or paste a kick.com link);
leave the app and it is the music search again.

### Signing in to Kick (optional) and importing your follows

Nothing in the Kick app needs an account. If you want your Kick follows in Hop-tunes' own follow list, open **account** on the streams page and press **sign in with a code**:
Hop-tunes shows a 6-digit code and an address on kick.com. Open the address in your browser (signed in to Kick) and approve it; Hop-tunes never sees your password. Then press
**import my follows**: the channels you follow on Kick are added to your follow list on this device (channels you blocked here are skipped, and nothing is followed or changed on
Kick). The session token Kick gives Hop-tunes is kept only in your profile on this device (a guest keeps it until the session ends) and is sent only to kick.com. **sign out**
forgets it; the imported follows stay. This uses Kick's unofficial TV sign-in, so it may stop working if Kick changes it.

### Opening a stream, full screen and the pop-out window (phone)

Tapping a stream on a phone asks **watch this stream?** first, so a slip of the finger while you scroll does not open one (settings → apps settings → **opening a stream** can turn the question off).
On the picture, the **full screen** button makes it fill the phone (the status and navigation bars hide), in portrait or landscape: turn the phone, or press **rotate** if your phone's own rotation is
off; the back button leaves full screen. The **pop out** button (or leaving Hop-tunes with the home button or gesture while a stream or download is playing) turns the picture into a small window that floats over your
home screen and other apps (Android's picture in picture): drag it anywhere, pinch to resize it a little, tap it to return to Hop-tunes. Android decides how small or large that window can be, and it only appears while something
is playing. It needs Android 8 or newer.

### On a phone: more room

Press the mini player at the bottom and slide it to the right to push it away; tap the little tab at the right edge to bring it back. The round arrow at the top right of the Kick app pulls
the menu (and on a channel's page the header, tabs and record button) out of the way. If a stream stops with an error, press **try again**; after switching to another app and coming back
the stream is asked for again automatically.

### Pulling the menu out of the way

On the streams page, the round arrow at the top right of the Kick app pulls the search box, filters and categories up like a curtain, so the streams get the room; press it again and
the menu comes back down. Searching with the phone's bottom search button also brings it back.

### Searching, and your own categories

Type a streamer's name in the search box and the list follows your spelling a moment after you pause (no enter needed), with the closest names first. If Kick finds nothing for a
phrase like "name vtuber", Hop-tunes searches its longest word and still puts the closest name first. Pasting a kick.com link opens that channel when you press enter.

The chips under the search box are Kick's popular categories. Press **edit** at the end of them (or open settings → apps settings → **kick categories**) to make them yours: take away the
ones you do not care about, search for a game or category by name and add it, move them up and down, or go back to Kick's popular ones. Finding categories needs the internet, so adding
works inside the Kick app; removing, moving and resetting also work from settings. The list is kept in your profile.

### English only

The **english only** switch next to the filters on the streams page (also in settings → apps settings → **kick language**) keeps the list to English streams. It is
saved in your profile and wins over the language menu while it is on. Streams whose language Kick does not state are kept. Searching for a streamer by name, your
follow list and your playlists are your own choices, so they are not filtered.

### Playlists

**playlists** on the streams page holds your own lists. Name one, choose what it holds (channels, clips or past streams) and press **create**. Then on a channel's page
use **+ playlist** to put the channel in a channels list; on the clips and past streams tabs use **+ list** on a row. Open a playlist to go to a channel, to see who is live
now (it only looks when you press **who's live now**), or to download and play the clips and streams in it. Playlists are saved with your profile on this device and are never sent anywhere
(guests keep nothing). A streamer you blocked stays hidden in your playlists. Up to 50 playlists with up to 500 items each.

### Downloading past streams and clips

Open a channel in Kick and use the tabs **live**, **past streams** and **clips**. Pick a quality (480p, 720p or best), then press **download** on
a finished stream or a clip; the list shows about how big it will be first (a long stream at 720p is several GB). Downloads are kept inside Hop-tunes' own
offline folder (see below), not as loose video files, and count against your offline storage limit. The **downloads** button on the streams page opens the
list of what you have: **play** it from your device, **pause**, **resume** or **delete** it. Downloads only run while the Kick app is open: leaving the app
pauses them and opening it again carries on. If the limit is reached or the drive is nearly full a download stops and says why; nothing is deleted. Only
finished streams can be downloaded; recording a live stream comes later. These are personal copies for watching offline: please respect the streamer's and Kick's rules.

### Recording a live stream

On a live channel's page press **● record** to keep the stream from that moment on. A red bar shows the time and size; press **stop** when you have enough.
It also stops by itself when the stream ends, when your offline storage limit is reached or the drive is nearly full (what was recorded is kept),
and when you close the Kick app (recording only runs while the app is open). Recordings appear in the **downloads** page and play from your device.
Quality follows the 480p / 720p / best choice on the past streams and clips tabs (720p by default). Recording starts from now: there is no rewind, and a
piece lost to a network drop stays lost. A long recording is large: watch the storage meter. Personal copies only.

### Kick offline storage

In **settings → apps settings → kick offline storage** you choose how much room Kick may use for things kept offline (downloaded past streams, clips and
recordings: these arrive in later versions). Type a number, pick MB or GB and press **set limit**; you can change it whenever you like. The meter shows how
much is used. When the limit is reached, or the drive is nearly full (Hop-tunes always leaves 1 GB free), new downloads pause and nothing is deleted:
raise the limit or remove something yourself. **choose a drive or folder** keeps it somewhere else, for example a bigger drive: what is already stored is
moved over, and if that drive is unplugged later Hop-tunes just says it is not available until it is back. It is saved with your profile (guests keep nothing).

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

**Pressing play does nothing and the list jumps to the last song.** Almost always the songs' files cannot be
reached: the drive they are on is not plugged in or not mounted, or the folder was moved. Plug the drive in, or
add the folder where the music really is in *settings → media folders*, then *rescan now*. Check that the folder you
added is the one that holds the music you are trying to play.

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
