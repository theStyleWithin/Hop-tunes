# **A local-first music player inspired by the Zune software**

*Missed the Zune software? This is for you.*

[![Your app | AlternativeTo](https://alternativeto.net/static/badges/badge-compact-dark.svg)](https://alternativeto.net/software/your-app/about/?utm_source=badge&utm_medium=referral)

**Hop-tunes** · Linux · GPL-3.0 · no accounts, no tracking

<p align="right"><img src="design/app/notesy-mascot.png" alt="Notesy, the Hop-tunes mascot, winking" width="220"></p>

> [!WARNING]
> **Feature complete. Download at your own risk.** Hop-tunes 1.2 includes every planned feature — M7 video playback included — and passes its automated test suite on 16 Linux distributions. The developer has now tested it on real hardware (a Steam Deck running CachyOS Handheld Edition, AppImage only) — that testing is what found and fixed a video-playback bug for this release. Other install methods and hardware are still untested by the developer, so if you choose to try it, you do so at your own risk.

> [!IMPORTANT]
> **ANNOUNCEMENT FOR HOP-TUNES APP DEVELOPERS: YOU ARE NOW A QUALIFIED APP DEVELOPER FOR HOP-TUNES!**
>
> Got an idea for an app? **Fork Hop-tunes** and **fork the [Hop-tunes apps AI developer toolkit](https://github.com/theStyleWithin/Hoptunes-apps-AI-dev-toolkit)**.
> The toolkit puts the power back in your hands to make all kinds of crazy projects you can dream of for Hop-tunes: if you can dream it, you can make it.
> Your AI reads the toolkit and reviews the official Hop-tunes, and builds your app only in your own fork of Hop-tunes.
>
> If you use Hop-tunes, you are already a developer, even if you have made nothing yet. If you use AI with Hop-tunes, even if you have never used it to build
> an app, you are a developer! That is all the skill you need to be an app developer for Hop-tunes.
>
> You do not need anyone's permission, and your app lives in your own fork and your own build (it is never added to the official Hop-tunes). The one rule:
> do not credit the developer of Hop-tunes with your work. The toolkit explains the rest.

## Install

### [Download the latest release](https://github.com/theStyleWithin/Hop-tunes/releases/latest)

The big link above always opens the newest regular release. Pick the file for your system from its **Assets** list (see the table below).

> [!TIP]
> **Just want the mobile version? Follow this link and enjoy!**
> **[Download the Android app (`Hop-tunes-android.apk`)](https://github.com/theStyleWithin/Hop-tunes/releases/latest/download/Hop-tunes-android.apk)**: it always gives you the newest one. It is for Android 7 and newer on 64-bit phones and tablets: your music, pictures, books and podcasts in a layout made for the phone, with a bottom bar or a side drawer (your choice in settings). Open the file to install it; Android may ask you to allow installs from this source. Videos play too (no subtitles or speed boost yet, and no thumbnails in the list). The phone version is new, so [tell the developer](https://github.com/theStyleWithin/Hop-tunes/issues) what works and what does not.

**visual music games** (Android)

The phone gets a **visualizer** on Now playing (portrait and landscape) and **Musy Note**: a little music note that flies through gaps in walls made of your album covers, with the walls timed to the beat. Tap to flap, or use a controller (the Red Magic Gravity X clip-on works: Start pauses, Back leaves). Auto-switching to Now playing when a controller connects is a setting too.

| visualizer | Musy Note (landscape) |
|---|---|
| ![phone visualizer](design/app/games-phone-visualizer.png) | ![Musy Note](design/app/games-musy-note.png) |

> [!NOTE]
> **Want to try Party Watch (beta)?** Party Watch is an optional, **experimental** idea: share part of your library with guests you trust, who stream it from your computer (no downloading, no accounts, nothing leaves your home unless you open it up), and watch together in step. It **needs more testing**, so it is **not** in the regular release above; the developer would like to add it to the main program once it has proved itself. If you are curious, try the **[Party Watch beta pre-release](https://github.com/theStyleWithin/Hop-tunes/releases/tag/v180beta)** (its files have `180beta` in their names). It is off until you switch it on, it has only been tested on Linux, things may change or break, and your feedback on how it works for you is very welcome.

> [!WARNING]
> **Windows: the developer never tests on Windows.** Hop-tunes is tested on Linux (and, for the phone version, on a real Android phone), not Windows. The Windows installer and portable exe are built and run by automated tests only, never by a person on a real PC. **Install at your own risk. Windows testers wanted:** please [open an issue](https://github.com/theStyleWithin/Hop-tunes/issues) to say what works and what doesn't.

| | |
|---|---|
| **Flatpak** | `Hop-tunes-x86_64.flatpak` from the [releases page](https://github.com/theStyleWithin/Hop-tunes/releases), then `flatpak install --user Hop-tunes-x86_64.flatpak` |
| **Arch / CachyOS (AUR)** | `yay -S hop-tunes` (or `hop-tunes-git` for the latest code) |
| **AppImage** | download, `chmod +x`, run. Any x86_64 distro from 2022 on |
| **Android 7+ (phones and tablets), new** | `Hop-tunes-android.apk` from the latest release (the link above). Music, pictures and video playback work (no subtitles yet). See [`docs/android.md`](docs/android.md) |
| **Windows 10/11 (64-bit), experimental** | `Hop-tunes-Setup-<version>-x64.exe` (installer), or `Hop-tunes-<version>-portable-x64.exe` (one file, no install; keeps its data in a `Hop-tunes-data` folder beside it) from the [releases page](https://github.com/theStyleWithin/Hop-tunes/releases). Not code-signed yet (if SmartScreen warns, choose *More info → Run anyway*), and only ever run by automated tests, never on a real PC by the developer (who uses Linux): reports welcome |

**Tested on 16 distros, with CachyOS first.** See [which distros Hop-tunes runs on](docs/COMPATIBILITY.md). Distros older than 2022 may not work with the AppImage; use the Flatpak there.

> **Heads up: keep your music where Hop-tunes can reach it.** If your music lives on an external drive, a USB stick
> or a network share, plug it in and let it mount *before* you press play, and check carefully in *settings → media
> folders* that the folder you added is the one that really holds your music (it is easy to have two copies, one in a
> folder that is connected and one that is not). With the drive unplugged every song is missing, so pressing play
> seems to do nothing and the list jumps to the bottom.

The [user guide](docs/user-guide.md) covers everything from the first run to troubleshooting.

## Docs

- [`docs/user-guide.md`](docs/user-guide.md): how to use Hop-tunes.
- [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md): tested distros (CachyOS first) and what to use on older ones.
- [`CHANGELOG.md`](CHANGELOG.md): what changed in each version.
- [`SPEC.md`](SPEC.md): product and technical spec: a local-first, login-free music player with local profiles.
- [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md): open-source components and their licenses; trademark notice.
- [`packaging/`](packaging/): AppImage, Flatpak and AUR build files.
- [`docs/RELEASING.md`](docs/RELEASING.md): how a release is made, checked and published, and the AUR/Flathub hand-off kits.

- [`design/`](design/): screenshots of the real app for each milestone, plus the original design mockups.

## Status

- **M1 (play a folder): done.** Incremental library scan, gapless playback, song list, now playing, mini player.
- **M2 (collection): done** except search. Artists → albums → songs panes, albums grid, cover art (embedded + folder images), MPRIS media keys and desktop media widgets.
- **M3 (local profiles): done.** "Who's listening?" picker, first-run profile setup, optional PIN (a privacy curtain, not security), accent colour and picture per profile, switch without restarting, guest mode that saves nothing. Each profile has its own folders and settings. No accounts or sign-in.
- **M4 (playlists & quickplay): done.** Instant search, hearts and play counts per profile, playlists (with move up/down), smart playlists with a rule builder, auto lists (hearted, most played, recently added, never played), playlist import (`.m3u`, `.m3u8`, `.pls`, `.xspf`, and Zune `.zpl`/`.wpl`, even ones made on Windows) and `.m3u8` export, play next / add to queue, and a Quickplay page with pins, history, new albums and an offline smart DJ.
- **M5 (now playing & polish): done.** Crossfade (0–12 s; albums stay gapless), even volume with ReplayGain (per song or per album), shuffle and repeat (also from desktop media widgets), a volume control, dark and light themes (or match the system), smooth transitions with a "reduce motion" option, drag-to-reorder for the queue and playlists, and Now Playing controls that fade away while you listen.
- **M6 (ship 1.0): done.** Released as [v1.0.0](https://github.com/theStyleWithin/Hop-tunes/releases/tag/v1.0.0). Flatpak (KDE 6.11 runtime, music folder read-only, other folders through the file chooser), AUR packages `hop-tunes` and `hop-tunes-git`, a user guide, changelog, man page, third-party notices, an About section with keyboard shortcuts, `--help`/`--version`/`--forget-pin`, opening files from a file manager (a second launch reuses the open window), and tagged GitHub releases with the AppImage and Flatpak attached. Version 1.0.0.
- **M7 (video): done.** A `collection › videos` grid (scanning, thumbnails, codec/resolution/HDR badges) and a video player with two engines: **libmpv** (richer subtitle styling, hardware decode via `hwdec=auto-safe`) where it's available, falling back to **GStreamer** (`qml6glsink`) where it isn't — currently the Flatpak build, since libmpv isn't packaged there yet. Both engines auto-load subtitles and remember a resume position. The GStreamer path was verified with real on-screen rendering (a real video, visibly playing); the libmpv path's render code follows mpv's documented API but couldn't be checked against a real GPU or display in the sandbox this was built in — see [`SPEC.md` §5b](SPEC.md#5b-video-modern-resolutions-including-4k) for exactly what's checked and what isn't. AppImage, AUR and a plain build all ship libmpv.
- **AppImage:** [download it from the latest release](https://github.com/theStyleWithin/Hop-tunes/releases/latest). Portable build for glibc 2.35+ distros, tested on 18 distro/display setups; see [`packaging/appimage/`](packaging/appimage/).

- **Game shelf:** `collection › games` is a bookshelf of covers for your Steam and GOG games and your emulator roms, with a chip per system (PC, Switch, Wii U, GameCube, N64, PSP, PS2, PS3 and more), your own titles, notes and cover pictures, and games from other launchers added by hand. It reads file names only and never launches anything; Epic Games libraries are not scanned. See the [user guide](docs/user-guide.md#games-the-game-shelf).

**On the horizon** (stretch goals, not started — see [`SPEC.md` §10](SPEC.md#10-possible-future-out-of-scope-for-v1)):
- An **Android port**, same local-only/always-offline vision with the UI revised for touch.
- New Collection sections for **audiobooks**, **pictures**, **ebooks** and local-file **podcasts**.
- A free **Steam listing**, mainly so Steam Deck users can launch it from Gaming Mode.
- More **apps**: see the [roadmap](#roadmap-more-apps-planned-nothing-below-is-built-yet) (Internet Archive, Odysee, DeviantArt and Blaze Stream are planned).

**New in M7: video**

| videos grid | playing (GStreamer fallback engine) |
|---|---|
| ![videos](design/app/m7-videos.png) | ![playing](design/app/m7-video-playing-gstreamer-fallback.png) |

**New in M5: polish, themes and playback options**

| light theme | now playing (drag to reorder up next) |
|---|---|
| ![light theme](design/app/m5-albums-light.png) | ![now playing](design/app/m5-now-playing-light.png) |
| **playback settings** | **appearance settings** |
| ![playback settings](design/app/m5-settings-playback.png) | ![appearance](design/app/m5-settings-light.png) |

**M4: playlists, quickplay and search**

| quickplay | playlists |
|---|---|
| ![quickplay](design/app/m4-quickplay.png) | ![playlist](design/app/m4-playlist.png) |
| **smart playlist rules** | **song menu** |
| ![smart playlist editor](design/app/m4-smart-editor.png) | ![song menu](design/app/m4-song-menu.png) |

**M3: local profiles**

| who's listening? | settings › profile |
|---|---|
| ![profile picker](design/app/m3-profile-picker.png) | ![profile settings](design/app/m3-profile-settings.png) |

**M1–M2: the music collection**

| collection › artists | now playing |
|---|---|
| ![artists](design/app/m2-collection-artists.png) | ![now playing](design/app/m2-now-playing.png) |

More renders, plus the original design targets, are in [`design/`](design/).

**visual music games** (desktop)

Optional extras on Now playing, under the **games** chip and the visualizer: a **visualizer** that moves with the song, **Album Fall**, falling blocks made of your own album covers where the beat of the song picks the piece and sets the speed, and **Cover Pinball**, a pinball table made from the cover of the song that is playing: the bumpers stand where the cover is brightest, in its colours, and the beat kicks them (a hit on the beat pays double). Play with the keyboard or a controller, or let the song play it for you (no score is kept then). Everything is off the moment you switch it off in Settings › visualizer & games, and the analysis only listens while one of them is on screen.

| visualizer | Album Fall (visualizer behind the game) |
|---|---|
| ![visualizer](design/app/games-visualizer.png) | ![Album Fall](design/app/games-albumfall.png) |
| **Cover Pinball** | **Cover Pinball (visualizer behind the table)** |
| ![Cover Pinball](design/app/games-pinball.png) | ![Cover Pinball with the visualizer behind](design/app/games-pinball-visualizer.png) |

**neko cafe** (a game in the apps tab, desktop and phone)

Hop-tunes has a little cafe of its own now. In **apps** you will find **neko cafe**: you are a neko maid running a tiny cafe. Customers walk in and sit, you take their order, hand the slip to the cook, grab the coffee when he holds it out and serve the *right* customer, fast. A quick serve earns a bigger tip, a slow one leaves them upset (and you earn nothing from them), and a few are grumpy no matter what you do. The day lasts five minutes, then the results show your orders, bills, tips, profit, happiness and a grade from A to D, and your profit goes on the best list. It is drawn in a classic handheld green, works fully offline, sends nothing anywhere and has **no music or sound of its own**: your music (or a stream playing in its small window) keeps going while you play. Arrow keys or wasd walk, space / enter / z is the A button, esc pauses; a game controller works, and phones get a touch pad under the picture.

| the apps tab: kick and neko cafe | the title screen |
|---|---|
| ![the apps tab](design/app/apps-tab-neko-cafe.png) | ![neko cafe title screen](design/app/neko-cafe-title.png) |
| a customer orders | the coffee is ready |
| ![a customer orders](design/app/neko-cafe-order.png) | ![picking up the coffee](design/app/neko-cafe-pickup.png) |
| the end of a good day | |
| ![the results of a day](design/app/neko-cafe-results.png) | |

*(These pictures are made by the test harness, which plays a scripted game; nothing in them is a real score.)*

**the Kick app** (apps tab, desktop and phone)

The other app is **Kick**: browse and watch live streams from kick.com inside Hop-tunes, with no account. It is **off until you switch apps on**, it **asks before it uses the internet** (the card lists every site it will contact), and it only connects while it is open. The streams page has a status light for Kick's servers (green, yellow or red), search as you type, category chips, language and "english only" filters, and your own **follows, blocks and playlists**, which live in your Hop-tunes profile on this device and are never tied to a Kick account (there is no sign-in). A channel page has live, past streams and clips: you can **download** a past stream or clip, or **record** a live one, into a storage limit you set, and watch it offline later. On a live stream the chat beside the picture is **none** or a clearly labelled **pretend** one you can post in; leave a stream and it shrinks to a small draggable window that keeps playing (desktop) until you close it or start a song or a video.

| the streams page | a channel: live, past streams, clips |
|---|---|
| ![kick streams page](design/app/kick-streams.png) | ![a kick channel page](design/app/kick-channel.png) |
| it asks first, and lists every site | downloads, kept on your device |
| ![kick asks before it connects](design/app/kick-asks-first.png) | ![kick downloads](design/app/kick-downloads.png) |
| on a phone: streams | on a phone: watching, with the pretend chat |
| ![kick on a phone: streams](design/app/kick-phone-streams.png) | ![kick on a phone: watching](design/app/kick-phone-watch.png) |

*(Rendered by the test harness with made-up channels and a made-up music library, so no real streamer appears here. Real streaming depends on Kick being up and on your video engine.)*

**On Android: the phone layout**

Renders of the Android app's phone layout (it adapts to tablets, and has a side drawer as an alternative to the bottom bar).

| music | videos | now playing | video player | video details |
|---|---|---|---|---|
| ![android music](design/app/android-music.png) | ![android videos](design/app/android-videos.png) | ![android now playing](design/app/android-now-playing.png) | ![android video player](design/app/android-video-player.png) | ![android video details](design/app/android-video-details.png) |

> **[Download the Android app (`Hop-tunes-android.apk`)](https://github.com/theStyleWithin/Hop-tunes/releases/latest/download/Hop-tunes-android.apk)**: the newest one, for Android 7 and newer on 64-bit phones and tablets. Open the file to install it; Android may ask you to allow installs from this source.

## Apps (new, off by default)

Hop-tunes now has an **apps** tab: small programs built into Hop-tunes that open inside its window. It is off until you switch it on in
**settings → apps settings**, so nothing changes for anyone who doesn't want it. The first app, **Kick**, browses and watches live streams
from kick.com with a follow list and a block list that stay on your computer (no Kick account needed). The second, **neko cafe**, is a little game
that works fully offline. The apps that use the internet (Kick, DeviantArt, the Internet Archive) and the optional **SteamGridDB cover picker** on the
game shelf are the only parts of Hop-tunes that do: each asks first, lists the exact sites it contacts, connects only while it is open, and every
connection goes through one gateway (secure connections, only those sites, no cookies, nothing kept; SteamGridDB also needs your own free key, which
goes only to SteamGridDB). Details in the [user guide](docs/user-guide.md#apps-off-until-you-switch-them-on).

### Roadmap: more apps (planned, nothing below is built yet)

These are the apps planned next, in this order. It is a plan, not a promise: the order and the details can change, and any of them can be dropped if it
cannot be done honestly. Every one would keep the promises Kick keeps: **off until you switch apps on**, it **asks before it uses the internet** and lists
the exact sites, it connects only while it is open and only through the one gateway (secure connections, plain reads, no cookies, nothing kept), **no
account or sign-in is needed**, and your follows, favourites and playlists live in your Hop-tunes profile on your device. A service that cannot be used
within those promises (for example one that needs a login, a wallet or a private, undocumented interface) is not built as an app, or it is built only if it
is clearly labelled and you choose it.

| app | what it would do | what I found so far, and the catch |
|---|---|---|
| **1. Internet Archive** ([archive.org](https://archive.org)) | **Built** (see the apps tab): search the Internet Archive's free music, films and books, play the music and watch the videos right inside Hop-tunes, read the books (EPUB, or the plain text of scanned ones), keep local favourites. The media preserver [normand64](https://archive.org/details/@normand64)'s picks are shown first, with a tag. | It uses archive.org's public search and download addresses, so no account and nothing to register. The curator row comes from his public favourites collection (his own uploads cannot be listed without an account), and only music, films and books are shown. Scanned books with only a PDF open on archive.org instead. Playback and reading are streamed, nothing is saved. What is on the archive belongs to its makers and not every item is free to reuse (each says its licence); Hop-tunes is not endorsed by the Internet Archive. |
| **2. Odysee** ([odysee.com](https://odysee.com)) | *(Dropped.)* Browse and search channels, watch videos, local follows and playlists (like Kick). | It was built and tested: Odysee's public search works without a login, but its video server refused to give any video file to Hop-tunes (HTTP 401, on a computer and on a phone), so nothing could be watched inside the app. Hop-tunes will not copy a browser's headers or sign in to get around that, and a browse-only app that sends you to the website was not worth shipping. It can be reconsidered if Odysee offers a documented public way to stream without a login. |
| **3. DeviantArt** ([deviantart.com](https://www.deviantart.com)) | **Built** (see the apps tab): browse artwork from DeviantArt's public feeds, look at it full size, with local favourites, blocked artists and words, and an 18+ switch (off at the start). | It uses DeviantArt's public RSS feeds, so no account and nothing to register. It is view-only (no offline saving: the art belongs to its artists), the feeds are not an official promise and can change, and blocked words are a word filter because the feed has no tag list. |
| **4. Blaze Stream** ([blaze.stream](https://blaze.stream)) | *(Dropped for now.)* A live-stream app like Kick. | Its site has no documented public interface, and its `robots.txt` tells automated clients to stay out of `/api/`, `/bapi/` and `/socket.io/`, which is where the live list and video would have to come from; the site also prompts for a wallet or login. Hop-tunes will not use paths a site asks automated clients to avoid, so it is not built. It can be reconsidered if Blaze publishes a documented public read-only interface. |

Each app would arrive the way neko cafe and Kick did: one small step at a time, with tests, renders you can look at, and an honest note on what could not be tried.

## Building

Hop-tunes is C++20 + Qt 6 (QML) + GStreamer + TagLib + SQLite.

**CachyOS / Arch**
```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative taglib sqlite \
    gstreamer gst-plugins-base gst-plugins-good gst-libav mpv
```

**Ubuntu 24.04 / Debian**
```sh
sudo apt install cmake ninja-build g++ pkg-config qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-dialogs \
    qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
    libtag1-dev libsqlite3-dev libmpv-dev libgstreamer1.0-dev gstreamer1.0-plugins-good gstreamer1.0-libav \
    gstreamer1.0-gl gstreamer1.0-qt6
```

`libmpv`/`mpv` is the primary video engine (spec §5b); without it, Hop-tunes falls back to
`gstreamer1.0-gl` + `gstreamer1.0-qt6` (GStreamer's own Qt video sink) instead — have
either one and video works, have neither and `collection › videos` still lists files, it
just can't open them.

**Build, test, run**
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
./build/hop-tunes
```

On first run Hop-tunes offers `~/Music`; add other folders under **settings**. Your data lives in `~/.local/share/hop-tunes/`.

## Research

- [`zune-teardown/`](zune-teardown/): structural teardown of the final Zune 4.8 software: installer format, component architecture, device and service map, and the open-source tools used to produce it. Groundwork for a clean-room implementation.
