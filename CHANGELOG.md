# Changelog

All notable changes to Hop-tunes. Versions follow [Semantic Versioning](https://semver.org).

## [Unreleased]

### Added
- **A seek meter on the video (Kick).** A wide meter along the top of the picture: drag it left to rewind or right to skip ahead, or tap **−10** / **+10**. For a downloaded stream or clip it spans the whole video. On a live stream it spans what Kick keeps behind "now" (about half a minute), and the **live** pill jumps back to the front; if a phone's player cannot rewind a stream it says so instead of showing a dead bar.
- **Pretend chat on downloaded streams.** Playing a download now has the same **none / pretend** chat as a live channel, beside the picture (under it on a phone); moving around in the video recalculates the chat for that moment.
- **Widescreen on a sideways phone.** Turn the phone sideways while watching a stream or a download and the bars around it go away: the picture takes all the room, the chat sits beside it, and a **‹** button leaves. Held upright again, everything comes back.
- **Full screen turns with the phone for a normal video** and stays upright for a tall, shorts-style one (Android).

### Fixed
- **Black screen in full screen and in the pop-out window (Android).** The picture is now set up again once the window has settled after full screen, pop-out or turning the phone. (Not yet confirmed on a phone: please try it.)

### Added
- **Kick: a health light.** The dot in the status bar is now green when Kick is up, yellow when it is slow or partly down, red when it is not answering. While the app is open it asks Kick two tiny questions (the list and a channel page, nothing about you) about once a minute, more often when something looks wrong. When it is red nothing live can be loaded or watched (a cover says so, with a button to your downloads), your downloads and recordings keep playing, and as soon as Kick answers again the list (and an open channel) refreshes by itself. The permission card says so up front.

### Changed
- **Kick: no sign-in, a paste box instead.** The optional "sign in with a code" (Kick's unofficial TV link, which some security scanners flag) is gone, together with the session token it kept and the special account requests it needed in the network gateway: apps inside Hop-tunes can now only ask for pages (GET), never send a body, a header or a password. **account** on the streams page became **add channels**: paste channel names or kick.com links and they go into your follow list or a playlist of channels, with nothing sent to Kick. A token an earlier version saved in your profile is ignored and dropped on the next save.
- **Kick: no disconnect button.** The yellow "disconnect" button in the status bar is gone, so a stray tap can no longer cut the connection. The bar still shows what is connected; taking the permission back in settings → apps settings → kick still closes it.

### Fixed
- **Kick stream pictures.** Kick sends every stream picture and profile picture as WebP, which Qt can only read through an optional plugin that the Android app (and some Linux installs) do not have, so the cards showed plain tiles. Hop-tunes now reads WebP itself (with libwebp), on desktop and, once the Android build carries it, on the phone.

### Added
- **Search tells gameplay streams from channel names.** After typing, three choices appear under the box: **everything**, **streams playing it** and **channels**. "Everything" lists the live streams playing a matching game or category first (under a "live streams playing palworld" heading) and the channels with a similar name after (under "channels named like..."); the other two show just one kind, and say so when there is nothing to show. Switching asks Kick for nothing new, and a blocked streamer is left out of both.

### Fixed
- The line above the search results was one long line that made the whole page wider than the phone's screen (cards went off the right edge); it wraps now.
- **Search finds streams playing a game.** Typing a game or category ("Palworld") now shows the live streams playing it first (most viewers first, with their pictures), then the channels whose names are similar, instead of only channels named like it. English only applies to those streams too.
- **Pictures stay.** Live search results get their stream picture (from what was seen before, or by asking for the live ones), channels that are not live show their avatar on the tile instead of a blank colour, channel playlists show a channel's live picture when it is live, and downloads and recordings keep their picture (fetched again the next time the app opens if it was missing).
- **More room for the chat on a phone.** The chat is taller, and the tab that brings back a pushed-away mini player is now a slim handle at the right edge that takes no room (it used to sit in its own row and squeeze the page).
- **Kick on a phone: ask before opening, full screen, and a pop-out window.**
  - Tapping a stream now asks "watch this stream?" first, so a slip of the finger while scrolling does not open one (settings > apps settings > opening a stream turns it off).
  - A full-screen button on the picture makes it fill the whole phone (status and navigation bars hidden), in portrait or landscape: turn the phone, or use the rotate button when the phone's own rotation is off. Back (or the button) leaves full screen.
  - Android's picture in picture: press the pop-out button, or leave Hop-tunes with the home button or gesture while a stream (or a download) is playing, and the picture pops out into a small window over the home screen and other apps. The system owns that window: drag it anywhere, pinch to resize it within Android's limits, tap it to come back.
- **Phone: push the mini player away.** Press the mini player at the bottom and slide it to the right to push it off the screen (more room, for a stream's chat for example); a small tab at the right edge brings it back.
- **A calmer Kick page on a phone.** The connection bar is now right under the app's title bar; on a channel's page the follow, list and block buttons have their own line, the chat always gets a usable amount of room (the page scrolls if the screen is short), and the round arrow at the top right also pulls the channel's header, tabs and record button out of the way.
- **Kick streams after switching apps (Android).** Switching to another app and back used to leave "the stream could not be played: Demuxing failed" (the stream's address is time-limited). Now the stream is let go while Hop-tunes is away and the channel is asked for again, with a fresh address, when you come back; any stream error also has a **try again** button.
- **A collapsible menu in the Kick app.** On the streams page the search box, filters and categories take a lot of room, especially on a phone. A round arrow at the top right of the app pulls them up out of the way like a curtain (the streams move up into the room) and brings them back when pressed again. Searching with the phone's search button brings the menu back down.
- **Kick search as you type.** The streams list follows your spelling a moment after you pause, no enter needed, closest names first (the exact name, then names that start with it, then ones that contain it, then near spellings). A phrase Kick finds nothing for ("name vtuber") is searched by its longest word and still ranked against everything you typed. One letter asks nothing; a pasted kick.com link still waits for enter.
- **Your own category chips.** Press **edit** at the end of the category chips (or see settings > apps settings > kick categories) to swap Kick's popular categories for the ones you care about: take one away, search for a game by name and add it, move them up and down, or go back to Kick's popular ones. Kept in your profile.
- **Hide the app descriptions.** Settings > apps settings > descriptions: the short line that pops up on an app's tile can be hidden for good (it also goes away by itself on a phone and no longer lingers over an open app).

### Fixed
- The tick boxes and little arrows in the Kick filter row showed as empty squares on some phones (missing font characters); they are drawn now.
- **Kick account (optional) and importing your follows.** On the streams page, **account**: sign in with a 6-digit code (you approve it on kick.com in your own browser; Hop-tunes never sees your password), then **import my follows** to copy the channels you follow on Kick into the follow list on this device. Nothing needs it to watch, search, download or record, and nothing is ever followed or changed on Kick. The session token is kept only in your profile on this device (a guest keeps it until the session ends) and is sent only to kick.com; **sign out** forgets it, and the imported list stays yours. It uses Kick's unofficial TV sign-in and has not been tried with a real account yet: if it stops working, Hop-tunes says so plainly.
- **Kick: english only.** A switch on the streams page (and under settings > apps settings > kick language) that keeps the streams list to English streams, whatever else you choose; the language menu steps aside while it is on. It is saved in your profile. Searching for a streamer by name, your follow list and your playlists are your own choices and are not filtered.
- **Playlists (Kick).** Make your own lists of favourite channels (or of clips, or of past streams): **playlists** on the streams page, then **+ playlist** on a channel (or **+ list** on a clip or past stream). Open a list to jump to a channel, see who is live right now (only when you press the button), or download and play the clips and streams you saved. Playlists stay on this device, in your profile, and nothing about them is sent anywhere; a blocked streamer stays hidden in them too.
- **Phone: the search button searches Kick while the Kick app is open.** On Android, with Kick open, the bottom "search" button takes you to Kick's streamer search (the search box is ready to type in) instead of searching your music; the button is highlighted while it is in use. Outside Kick it is the music search as before.
- **Record a live stream (Kick).** A live channel's page has a **record** button (never automatic). It keeps the stream from that moment on, in your offline downloads, until you press **stop**, the stream ends, your storage limit or the drive is nearly full (it then stops cleanly and keeps what it has), or you close the Kick app. A red bar shows how long and how big the recording is; finished recordings are in the downloads page like the rest and play from your device. Recording starts from "now" (no rewind), and a long recording is large, so keep an eye on your storage limit.
- **Download past streams and clips (Kick).** A channel's page has three tabs: live, past streams and clips. Press **download** on a finished stream or a clip (quality: up to 480p, 720p or the best) and Hop-tunes keeps it inside its own offline folder, counted against your offline storage limit, to watch without the internet. It is not saved as a loose video file. A **downloads** page lists what you have, with progress, play, pause, resume and delete. Downloads only run while the Kick app is open (leaving it pauses them, opening it again carries on), and they stop, never delete, when the limit is reached or the drive is nearly full.
- **Room set aside for offline Kick.** Settings > apps settings > kick offline storage: you choose how much space Kick may use for things kept offline (downloads and recordings come next), in MB or GB, and you can raise or lower it at any time. A meter shows what is used, with a warning at 80%. At the limit, or when the drive is nearly full, new downloads pause; nothing is ever deleted to make room. Optionally keep it on another drive or folder: what is already stored is moved there, and an unplugged drive just shows as "not available" until it is back. The setting is per profile.
- **Kick on Android.** The phone version of Hop-tunes now has the Kick app too, with the same promises: off until you switch apps on, it asks before it uses the internet, only the listed sites, no cookies, nothing kept, follow and block lists stay on the phone. One column of stream cards, the picture on top with the chat underneath on the watch page, and the video plays through Android's own player. The Android app now carries the internet permission (and OpenSSL, for secure connections); a build made without the networked apps has neither.

## [1.9.0] - 2026-11-12: Giving power back to the ordinary

### Added
- **Kick, the first app (desktop).** Browse the live streams on kick.com in a wide menu (nothing plays until you pick one), watch one inside Hop-tunes, and keep a follow list and a block list that stay on this computer. It is off until you switch apps on in settings, and it asks before it uses the internet, listing exactly which sites it contacts. Everything it reaches goes through one gateway: secure connections only, only those sites, no cookies, no referrer, nothing kept after you close the app, and a bar that shows what is connected with a disconnect button. When the developer (gamehopper) is live, that stream takes the first card of the default feed, marked "developer" (your block and 18+ choices still win). No Kick account is needed, and Hop-tunes never follows or subscribes for you on Kick.

### Added
- **Apps (desktop).** A new collection tab, next to pictures, audiobooks, ebooks and podcasts: small apps that are part of Hop-tunes and open inside its window, each one a glass tile. The tab is off until you switch it on in the new **settings > apps** section, so a fresh Hop-tunes shows every other tab and stays as quiet as before. There is one sample app for now; real ones come one at a time. The music keeps playing inside an app, and Esc or the back button leaves it.

## [1.8.0] - 2026-11-06

### Added
- **Cover Pinball (desktop).** A pinball table made from the cover of the song that is playing: the bumpers stand where the cover is brightest and take its colours, the beat of the song kicks them (a hit on the beat pays double) and the bass changes how heavy the ball feels. Flippers on Z and / (or left and right, or a controller's LB and RB), space to launch, three balls, a best list per profile, and "let the song play" plays it for you without keeping a score.

### Changed
- **Meet Notesy, the Hop-tunes mascot.** The little note from Musy Note, winking, is now the app icon on the desktop (Linux, Windows) and on Android (adaptive, round and legacy icons).
- **Now playing: "album fall" became "games".** The chip opens the last game you played, and a small picker offers Album Fall and Cover Pinball.

### Fixed
- Clicking a button in a game (pause, let the song play) no longer takes the keys: Space used to press that button again instead of dropping the piece or launching the ball.

## [1.7.5] - 2026-11-06

### Added
- **Android: game controllers, the visualizer and a game, Musy Note.** Hop-tunes now sees game controllers (USB, Bluetooth,
  clip-on ones like the Red Magic Gravity X) and reads their buttons. With a controller connected, starting a song in the
  collection, or plugging the controller in while one plays, opens Now playing (setting: "controller"). Now playing on a
  phone has a **visualizer** (bars, mirror or rings, in your accent, a rainbow or white; the same one as on the desktop) in
  place of the cover, in portrait and landscape, and a **games** button for **Musy Note**: a cute music note that flies
  through gaps in walls made of your album covers. The walls reach the note on the beat of the song, the tempo sets the pace
  and the pitch decides where the gap is. Tap, or press any button on a controller, to flap; "let the song play" flies it for
  you (and then no score is kept). Best scores are kept per profile. On Android the visualizer and the game listen to the
  song through an optional sound tap that is only on while one of them is on screen; if a song ever fails with it on, it is
  taken out and the song retried, if the app ends while it is on it stays off next time, and Settings has a "visualizer &
  games" switch. Music playback never depends on it.
- **Landscape Now playing on phones**: the cover on the left, the song and buttons on the right.
- **Media on a drive that is not plugged in is left out, and comes back by itself.** Music, videos, books, podcasts and
  pictures in a folder that cannot be reached (an external drive, a USB stick, a network share that is away) no longer show
  as songs that cannot play: they are simply not listed until the drive is back, and Settings marks the folder "not
  connected". Every few seconds Hop-tunes notices a drive coming or going and scans what came back. Nothing is deleted.
- **Album Fall (desktop): a pause button and "let the song play".** The game now has an on-screen pause button (P still works),
  and "let the song play" hands the controls to an autopilot that plays on the rhythm of the song, turning, sliding and
  dropping each piece. Nothing is scored while the song plays: a game it played never goes on your best list.
- **Live chat: whoever types is a member.** Your own messages in the pretend live chat (films and Now playing) carry the
  MEMBER badge, because nobody pays for a pretend stream.

### Fixed
- **Now playing on a narrow phone** (portrait) was cut off at the right and the bottom: the buttons at the top, the cover and
  the controls now fit, and the cover shrinks on a short phone.
- **A game controller whose stick rests off-centre could press buttons by itself** the moment it was opened (the "resting
  state" Linux reports at start was taken for presses).

## [1.7.4] - 2026-11-05

### Added
- **Games and a visualizer in Now playing (desktop).** Next to "the song" there are now two more views, switched with
  the buttons at the top (or the G key): a **music visualizer** (bars, a mirrored skyline or rings; colour from your
  accent, a rainbow or white; a strength slider; your choices are remembered with your profile) and **Album Fall**,
  a falling-blocks game where every block is one of your album covers and the music runs the game. A beat in the song
  chooses the next piece (a soft beat gives a small piece, a hard one a big five-block piece), the tempo and the bass set
  how fast things fall, and landing a piece right on the beat scores a bonus and a streak that multiplies line clears. If
  a song has no clear beat the game keeps its own pulse. Play with the keyboard (arrows or WASD, up or X to turn, Z to
  turn back, space to drop, P to pause, Esc to leave) or a game controller (d-pad or stick, A/RB turn, B/LB turn back,
  X drop, start pause, back leave; Linux and Windows). Your best five scores are kept per profile. The visualizer can
  also play faintly behind the game. The sound is only analysed while one of these views is open. (On Android the
  views are not available yet.)
- **Optional live chat in Now playing.** A "live chat" switch at the top shows the pretend live-stream chat beside the
  song or the game, talking about the song that is playing (its title, artist, album, year). Nobody in it is real, it
  is off until you switch it on, and what you type gets an answer from a chatter or two, like the chat for films.
- **Folder slots in videos, audiobooks and podcasts**, like pictures already had. Every video category (movies,
  shows, cartoons, anime, other videos), audiobooks and podcasts now show one slot per folder (a cover or the folder's
  first letter, its name and how many items it holds) instead of one big pile. Open a slot to see just that folder's
  items, use "‹ folders" (or the phone's back button) to return, and "rename" gives a slot a name of your own: the
  folder on disk is never renamed, and the name is remembered (and travels with an exported profile). Ebooks and the
  other sections are unchanged.
- **Fix wrong artist names in your collection.** Right-click an artist: **merge into another artist…** (pick the
  one that takes over, with a swap button and a confirm step), **rename artist…** (a name that is already in the
  list merges the two), and **split back…** to undo. You can also drag an album from an artist's albums onto another
  artist in the list to move just that album, and repeat for the next. The artist you merge into takes over: its name
  and cover are shown and albums with the same title combine into one. Your edits to the albums (title, cover, track
  order, quickplay pins) follow them, nothing is deleted, your music files are never changed, and it all travels
  with your exported profile. Desktop; phones don't have the artist list menu yet.

## [1.7.3] - 2026-11-04

### Added
- **Eject profile, save a copy, and import a profile** (desktop and Android). Settings › "take your profile with
  you" saves a plain `.hoptunes.json` file with your name, colour, picture, settings, playlists, hearts, play counts,
  history, pins, resume points, edits and comments. "save a copy…" just writes it. "eject profile…" saves it,
  reads the file back to check it, and only after you tick "I have the file and it is safe" removes the profile
  from the device. Importing is on the profile picker ("import a profile…"): it checks the whole file first,
  never overwrites a profile (a taken name is refused), lets you pick a new name and a PIN, and lists any music
  folders that are not on this device. The PIN is never put in the file, and music, videos and pictures stay where
  they are. Cover and thumbnail pictures you picked are not exported (their files live on the old device).
- A short, optional support note at the very bottom of Settings (a tip link and a Locals page; no popups, nothing
  nagging, and no shame in freeloading).
- **Video playback on Android.** Films, shows and the rest now play on the phone: the picture, play and pause,
  jump back or forward 10 seconds, tap the bar to seek, and your place is remembered and resumed. Volume stops at
  100 % there (the boost past 100 % is desktop only), and subtitles are not on Android yet. Films still show without
  a length or a picture in the list on Android.

### Fixed
- **Android: the app going completely grey after pressing "back".** The back button used to close the app's
  screen and leave a grey window behind. Back now steps back through the app instead (a song list to its albums to
  the artists, a picture or book closes, a film's page closes, settings go back to profile) and never leaves the
  app; at the top of the music collection it does nothing, and from another collection tab it returns to music.
  Desktop is unchanged.
- **Android: Hop-tunes had no icon on the home screen / app list.** It now has the same icon as the desktop app
  (a round / adaptive version for newer Android).
- **Android: music did not move on to the next song (or repeat) while the screen was off, until you woke the
  phone.** Android pauses the app's main loop in the background, and the queue was driven from that loop. The
  hand-off to the next song now runs on the music player's own thread, so loop and the queue keep going with the
  screen off. (With crossfade switched on, songs from different albums still wait for the screen.)
- **Android: adding a folder turned the whole screen grey and left the app unusable.** The folder chooser Qt provides
  has no Android version, so it drew a dark layer over everything and nothing could be tapped. Android now opens the
  system's own folder chooser instead; desktop is unchanged. (A first fix was not enough on one phone, so this
  patch also repaints the window when you return from the chooser and starts the scan only after that first paint.)
- **Android: "Hop-tunes isn't responding" after choosing a folder.** The real cause: right after a folder was
  chosen, the app walked the whole folder tree and set up a live-update watch on every sub-folder, on the thread
  that draws the screen. A big music (or storage) folder kept that thread busy for so long that Android reported the
  app as frozen. Phones no longer watch folders live (they scan at start-up and on "rescan now"); desktop keeps
  live updates.

### Changed
- **Phones: the video player keeps the picture clear.** The round buttons no longer sit over the film. Held
  upright, only the close arrow, the time and a thin progress bar are on the picture; play / pause sits just
  under it, rotate and subtitles are at the right end of the title row, and the button row has play, back 10 s,
  subtitles, forward 10 s and volume. Turned sideways, the controls show when you tap the picture and hide
  again after a few seconds while the film plays.
- **Phones: the close arrow, rotate and search icons are drawn instead of typed**, so they no longer show as
  empty boxes on phones whose font lacks those symbols.
- **Phones: the collection stays together during the first scan.** The title, tabs, "scanning…" note and the
  albums / artists / songs / playlists pills used to be spread far apart down the screen while the library was
  being scanned for the first time; they now sit together at the top.
- **Phones: you make your profile first and choose your music folder afterwards**, in the collection, instead of
  on the profile form (the profile still starts with the phone's Music folder). Desktop is unchanged.

### Added
- **Settings › diagnostics on phones:** a small log of what the app did and any errors, kept on the phone only,
  with buttons to show it and copy it (this run or the last one), so a problem can be reported without a terminal.

## [1.7.2] - 2026-10-30

### Added
- **An Android app (first version).** Hop-tunes now builds for Android phones and tablets: your library screens,
  music playback, and a navigation you can switch in settings between a **bottom bar** and a **side drawer** (a
  side rail or permanent drawer on tablets), with the phone screens laid out as in the design mockups. Video
  playback is not on Android yet (the video screens are there, the picture is not). See `docs/android.md`.

## [1.7.1] - 2026-10-30

### Fixed
- **Closing Hop-tunes during a video scan no longer makes the next scan start over.** New videos are now
  listed straight away, so shows and films appear while the scan is still working, and a scan that was cut
  short carries on with only the files it had not finished. The lists refresh every couple of seconds while
  it runs.
- **Videos that had no thumbnail now get one.** A film whose picture could not be grabbed (or whose cached
  picture was cleared) was never tried again; later scans now retry it.

## [1.7.0] - 2026-10-30

### Added
- **A private status box** at the top right of the collection view, right above your profile: an empty
  box for anything you want to see there, like a lyric, a quote or how you feel. Nobody else ever sees
  it; it is saved in your profile (up to 300 characters) as you type and is there next time you open
  Hop-tunes. Guests do not get one, since a guest keeps nothing.
- **Picture albums.** `collection › pictures` now shows one slot per folder (a cover, the folder's
  name and how many pictures it holds) instead of pouring every picture into one big grid. Click a slot to
  open its pictures, and "‹ albums" to go back; the viewer, arrow keys and slideshow work inside the open
  album. Add several folders and each stays its own album. Slots can be **renamed** ("rename" on the slot
  or in the open album; clearing the name goes back to the folder's name), and the name is remembered per
  profile, across restarts and rescans.
- **A movie volume that is separate from the music volume.** The video player has its own volume slider
  (and a mute button) that goes from 0 to **150 %**: past the mark at 100 % it boosts quiet films. It is
  saved per profile and never touches the music volume. Keys: `↑` / `↓` change it by 5, `m` mutes.
- **Video categories.** The videos tab now has sub sections like music does: **movies, shows, cartoons**
  (western animation), **anime** and **other videos** (for things like videos you saved from YouTube to
  keep). Each has its own folders and its own "choose a folder" button (in the tab and under
  settings), and a video belongs to the category whose folder is its closest parent, so a folder
  inside a movies folder can still be added as anime. Your existing video folders are your movies, so
  nothing moves. The "up next" list while watching follows the category you are in.

### Changed
- **Search now sits right after the tabs.** In the collection view the search box (with the song count after
  it) moved from the far right to just after "podcasts" in the music · videos · pictures row. Nothing
  else about the menu changed.
- **Your profile moved down.** The profile name and picture are at the right end of that tab row, where
  search used to be, in the collection view. In settings, quickplay and the other views the profile stays
  in the top bar.

## [1.6.0] - 2026-10-30

### Added
- **A page for every movie.** Clicking a tile in `collection › videos` now opens the movie's page instead
  of playing it; playing is a button on that page ("play", or "resume from 12:30"). The page has empty
  boxes for you to fill in: title, year, rating, score, tagline, description, genre, director,
  writers, studio and tags, a cast and crew list (name, role and an optional photo for each person), a
  poster (the same picture as the grid tile; "change poster" / "reset") and a "mark watched" toggle.
  Nothing is looked up or detected for you; only facts from the file itself are shown (length,
  resolution, codec, size). A title you type replaces the file name in the grid and the player.
  Everything is saved per profile and private to it. Finishing a movie marks it watched.
- **Three ways to watch: window, theater and fullscreen**, and fullscreen is no longer the default.
  *Window* shows the video with your other videos in an "up next" list beside it and the movie's
  details (what you filled in on its page) underneath; *theater* stretches the video across the
  window with the details and the list below it; *fullscreen* fills the screen. Switch while watching with
  the "theater" and "fullscreen" buttons or the `t` and `f` keys (`esc` steps back, and `space` pauses).
  Choose the one you start in under **settings › video settings › viewing style**; it is saved per
  profile. Picking another video from the list switches to it, keeping your place in the first.
- **Comments under videos** (settings › video settings › comments; window and theater only, never fullscreen).
  Three choices: *off* (the default; the section is gone completely, for anyone who finds it bloat),
  *only mine* (a box where you can write as many comments as you like and nobody ever answers; they are
  saved per video in your profile and can be deleted), and *populated* (made-up commenters with likes and
  replies, sorted by "top" or "newest first", "show more comments" never runs out, and a few of them answer
  what you write after a little while). Like the chat, nobody is real, everything is generated on your
  computer from the details you filled in on the movie's page, and only your own comments are saved.
- **Livestream chat: a pretend live chat beside the video** (settings › video settings › livestream chat, off by
  default; a separate switch from the viewing style, so it works with window, theater and fullscreen). Nothing is
  streamed and nobody in it is real: the chatters are generated on your computer from the video and the second you
  are at, so it is the same every time and seeking just recalculates it. They react to the details you filled in on the
  movie's page (title, year, genre, director, cast and roles, studio, tagline, tags) and answer when you type: ask who
  directed it and they tell you if the page says. They are friendly and generic ("sane mode"). Two looks, **clean**
  (round avatars, plain text) and **lively** (colours, badges, emote chips). Toggle it while watching with the "chat"
  button or `c`. Nothing is saved; the chat is marked as pretend.
- **Livestream chat: a pretend live chat beside the video** (settings › video settings › livestream chat, off by
  default; a separate switch from the viewing style, so it works with window, theater and fullscreen). Nothing is
  streamed and nobody in it is real: the chatters are generated on your computer from the video and the second you
  are at, so it is the same every time and seeking just recalculates it. They react to the details you filled in on the
  movie's page (title, year, genre, director, cast and roles, studio, tagline, tags) and answer when you type: ask who
  directed it and they tell you if the page says. They are friendly and generic ("sane mode"). Two looks, **clean**
  (round avatars, plain text) and **lively** (colours, badges, emote chips). Toggle it while watching with the "chat"
  button or `c`. Nothing is saved; the chat is marked as pretend.

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
