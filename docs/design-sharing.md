# Design: sharing a library with family (draft 2, for review)

Status: **draft, nothing is built.** Goal: let one person (the *host*) let a few people they trust (the *guests*,
first of all the host's dad) watch and listen to part of their Hop-tunes library. Not a media server: both sides
run Hop-tunes, and the host decides, item by item, what each guest may see. **Streaming only, never downloading.**

## Principles
- **Off by default, and truly off.** While sharing is off, no network code runs and nothing listens.
- **Nothing is visible until the host says so (the waiting room).** A guest who connects sees an empty library and
  a message that the host has not shared anything with them yet. The host approves content **one item at a time,
  per category**; everything else stays hidden. This is default-deny: new files you add later are *not* shared
  until you approve them.
- **Approvals are remembered, per guest.** The next time that guest connects they see exactly what was approved,
  no more. The host can add or remove items for a guest at any time, and removing takes effect at once (a
  stream in progress is cut).
- **Stream only. No downloads, by design.** There is no download button, no download endpoint, and the guest's app
  never writes the media to disk. (See "Honest limits" for what this does and does not mean.)
- **Read-only, and only what was approved.** Guests never see profiles, the status box, comments, file paths or
  anything not approved. Items are addressed by opaque ids, never paths.
- **Trusted people, not the public.** No public listing, no discovery of strangers, no accounts, no central server.
- **Plain to see.** A visible "sharing" indicator, a list of who is connected, and one click to stop or to kick.

## All content types
Music, videos, pictures, audiobooks, ebooks and podcasts can all be shared the same way, each with its own
approval list. Music and video come first, because they are what people will use most; the others follow the same
rules (pictures and ebooks are stream-to-view: shown, never saved).

## What the host controls (Settings → Sharing)
- The master switch (off by default).
- **Guests:** a list with name, last seen and **remove**. Opening a guest shows their approvals, per category.
- **Approving:** inside the host's own collection, a "share with…" choice on an item (or a selection), or from the
  guest's page: browse a category and tick what that guest may see. Because a library can have thousands of
  items, there are host-side shortcuts, each still an explicit action by the host: approve a whole album, show,
  folder or category, or approve everything matching a search. Nothing is ever approved automatically.
- **Pending view:** items you have not decided about, so new additions are easy to review.
- **Invites:** a one-time code (8 characters) with an expiry (10 minutes to 24 hours). Wrong guesses are rate
  limited and lock the invite after a few tries. After the first code a guest keeps a private key until removed.
- **Limits:** how many guests at once, how many streams at once, and a bandwidth cap.
- Whether the movie details you filled in are shown with approved movies (on by default, optional).

## What the guest sees
- A **"shared with me"** area: while nothing is approved, an empty "waiting for the host" screen. Once items are
  approved, the same grids and pages as the host's, read-only, with only the approved items. Play streams from the
  host.
- Watch progress stays on the guest's own machine and is never sent to the host.

## Watch together (a later phase)
A "watch party": the host picks something and plays it, and guests who are approved for it follow along in sync
(play, pause and seek follow the host). Each guest still streams their own copy from the host, so it needs the host's
upload speed for every guest. Not in the first version.

## How it connects
- The host app runs a small HTTPS server only while sharing is on. HTTP range requests are enough for seeking.
  Endpoints (all need the guest's key and check the approval for every request): `hello`, `library` (only
  approved ids, titles, details), `thumb/<id>`, `stream/<id>` (range) and `subtitles/<id>`. **There is no endpoint
  that returns a whole file as a download, and stream links are short-lived.**
- The invite carries the host's address and the **fingerprint of the host's certificate**, so the guest checks they
  reached the right machine without a certificate authority.
- The guest app fetches through a small local proxy that checks the fingerprint and hands the player (mpv or
  GStreamer) a plain `127.0.0.1` address. The proxy keeps only a small in-memory buffer, nothing on disk.
- **Same network:** the host is found automatically (mDNS) and its address is also in the invite.
- **Over the internet (the hard part):** home routers block incoming connections. Options, in order:
  1. A VPN both sides install (Tailscale or ZeroTier, free): the two machines look like one network, and no router
     changes are needed. Recommended for dad.
  2. Port forwarding on the host's router, with step-by-step instructions in the app.
  3. UPnP (automatic port opening) later, if it proves reliable.
  4. A relay or rendezvous server: not planned, because it means someone running a server.
- By default the server only accepts connections from the local network; "allow internet" is a separate switch.

## Honest limits
- **"No downloading" is a design choice, not a lock.** Hop-tunes will not offer, save or cache a copy. But anything
  you can watch is, in principle, something a determined person could capture (for example by recording the screen
  or the network stream). There is no DRM here and I would not claim there is.
- **Legal.** Streaming only, approval of each item by the host, no public listing and no strangers all fit "private
  sharing between people who already know each other", and they are sensible things to build. I cannot tell you
  that they protect the project from a complaint, and I am not a lawyer. If this is a real worry, it is worth a
  short conversation with someone qualified before the feature ships, and the README should say plainly that
  sharing is for personal and family use and that what is shared is the host's responsibility.
- **No transcoding at first.** The guest streams the file as it is, so the host's upload speed and the guest's
  hardware must cope. A large 1080p file can need 20-50 Mbit/s. Music streams are small and are fine on almost any
  connection.
- Windows will ask the host about the firewall the first time. Android and the Windows build are untested by the
  developer, so sharing is tested on Linux first.

## Phases
1. **LAN sharing, music and video:** host switch, waiting room, per-guest per-item approvals (remembered), invite
   codes, guest browse and stream, remove, limits.
2. **Internet use:** the address in the invite, VPN and port-forward instructions, the "allow internet" switch.
3. **Other sections:** pictures, audiobooks, ebooks, podcasts, with the same approvals.
4. **Watch together**, subtitles and audio-track choices, and (maybe) transcoding.

## Testing
- Unit tests: invite and key handling, rate limiting and lockout, default-deny (an unapproved id is refused even if
  guessed), approvals are per guest and survive a restart, revoking cuts a stream, id-to-path mapping, range
  requests, the bandwidth cap, and that no endpoint serves a whole file as a download.
- A two-instance test on loopback (host and guest in one test run). CI has no internet, so nothing needs it.
- A security review of the server code before it ships: it is the first part of Hop-tunes that listens for
  connections.

## Questions that are still open
1. Is the approval shortcut (approve a whole album, show, folder or category, or everything matching a search, as
   an explicit action by the host) acceptable, or must it really be strictly one item at a time?
2. Is "watch party" the same-time viewing described above (everyone follows the host's playback)?
3. Is "install Tailscale" acceptable for your dad over the internet?
4. Should the host be able to see what a guest is playing right now?
