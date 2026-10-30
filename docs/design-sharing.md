# Design: sharing a library with family (draft for review)

Status: **draft, nothing is built.** Goal: let one person (the *host*) share part of their Hop-tunes library
with a few people they trust (the *guests*, first of all the host's dad). Not a media server: both sides run
Hop-tunes, and the host opens a "room" with rules, a bit like an online lobby.

## Principles
- **Off by default, and truly off.** While sharing is off, no network code runs and nothing listens.
- **Read-only, and only what the host picked.** Guests never see profiles, the status box, comments, file paths or
  anything that was not shared. Items are addressed by opaque ids, never paths.
- **Trusted people, not the public.** No public listing, no discovery of strangers, no accounts, no central server.
- **Plain to see.** A visible "sharing" indicator, a list of who is connected, one click to stop or to kick.

## What the host controls (Settings → Sharing)
- The master switch (off by default).
- What is shared: pick from movies, shows, cartoons, anime, other videos (videos only in the first version).
- Invites: a one-time code (8 characters) with an expiry (10 minutes to 24 hours). Wrong guesses are rate limited
  and lock the invite after a few tries.
- Guests: a list with name, last seen and a **revoke** button. After the first code, a guest keeps a private key,
  so they do not type a code again until revoked.
- Limits: how many guests at once, how many streams at once, a bandwidth cap (so a movie does not eat the whole
  upload), stream only or allow download.
- Sharing the movie details you filled in (title, year, cast and so on) with each movie: on by default, optional.

## What the guest sees
- A new **"shared with me"** area in the videos tab: the host's name, then the shared categories as the same
  grids and movie pages (read-only). Play streams straight from the host.
- Watch progress and "watched" marks are kept on the guest's own machine, never sent to the host.

## How it connects
- The host app runs a small HTTPS server only while sharing is on. HTTP with range requests is enough for seeking.
  Endpoints (all need the guest's key): `hello`, `library` (ids, titles, details), `thumb/<id>`,
  `stream/<id>` (range), `subtitles/<id>`.
- The invite carries the host's address and the **fingerprint of the host's certificate**, so the guest checks
  they reached the right machine without a certificate authority.
- The guest app fetches through a small local proxy that checks the fingerprint, and hands the player (mpv or
  GStreamer) a plain `127.0.0.1` address. That avoids teaching the players about pinned certificates.
- **Same network:** the host's address is found automatically (mDNS) and also shown in the invite.
- **Over the internet (the hard part):** home routers block incoming connections. Realistic options, in order:
  1. **A VPN both sides install** (Tailscale or ZeroTier, free for this): it makes the two machines look like
     they are on one network, and needs no router changes. Recommended for dad.
  2. **Port forwarding** on the host's router, with step-by-step instructions in the app.
  3. UPnP (automatic port opening) later, if it proves reliable.
  4. A relay or rendezvous server: not planned, because it means someone running a server.
- By default the server only accepts connections from the local network; "allow internet" is a separate switch.

## Honest limits
- **No transcoding at first.** The guest streams the file as it is, so the host's upload speed and the guest's
  hardware must cope. A large 1080p file can need 20-50 Mbit/s. Transcoding (what Jellyfin does) is a big job and
  not in phase 1.
- Windows will ask the host about the firewall the first time. Android and the Windows build are untested by the
  developer, so sharing is tested on Linux first.
- This is for personal and family use; what is in a library and who sees it is the host's responsibility.

## Phases
1. **LAN sharing, videos only:** host switch and rules, invite codes, guest browse and play, revoke, limits.
2. **Internet use:** the "address" in the invite, VPN and port-forward instructions, "allow internet" switch.
3. Later, if wanted: share pictures, audiobooks, ebooks and podcasts; subtitles and audio track choices;
   transcoding; a relay.

## Testing
- Unit tests: invite and key handling, rate limiting and lockout, revoke, id-to-path mapping, range requests,
  bandwidth cap.
- A two-instance test on loopback (host and guest in one test run): browse, play a range, get refused after
  revoke. CI has no internet, so nothing here needs it.
- Security review of the server code before it ships, because it is the first thing in Hop-tunes that listens
  for connections.

## Open questions for the host
1. Videos only first, or pictures and audiobooks too?
2. Should the guest be allowed to download (save) a movie, or stream only?
3. Is "install Tailscale" an acceptable answer for dad over the internet, or must it work without it?
4. Should the host be able to see what the guest is watching right now (and when)?
