# 06: Implications for Hop-tunes (Linux re-implementation)

How each Zune subsystem maps to a Linux replacement that can be written cleanly, from public specs and existing open-source libraries.

| Zune subsystem | Linux / open replacement | Difficulty |
|---|---|---|
| UIX UI + ZuneShell | Own UI. Qt/QML or GTK4/libadwaita can reproduce the Metro look: big type, pivots, panoramic scrolling | Medium (design work) |
| SQL CE library DB (ZuneDB/QP/SE) | SQLite + a file watcher (inotify) + tag reading via TagLib | Low |
| Codecs (H.264, AAC, MPEG-4, MP3, WMA/WMV) | GStreamer or FFmpeg (libav*) | Low |
| Gapless / time compression / visualiser DMOs | GStreamer `playbin3` gapless, `scaletempo`, spectrum element | Low |
| Transcoding for sync (ZuneEnc) | FFmpeg / GStreamer encode pipeline to device-supported profiles | Low–Medium |
| `.zpl` playlists, `.m3u` | XML/SMIL parser; `.m3u` is trivial | Low |
| UPnP/DLNA sharing (ZuneNss) | Rygel, or gupnp-av / libupnp. ContentDirectory is standard | Low |
| Zune Wi-Fi `Handshake` service | Undocumented; needs protocol capture | High |
| Device sync over USB (WMDM + UMDF + MTP) | libmtp / libusb. MTP itself is a public USB-IF spec | Medium |
| **MTPZ authentication** | libmtp has an MTPZ code path, but it needs per-device cryptographic material that Microsoft never published. This is the **hardest blocker** for syncing real Zune hardware | High |
| On-device DB (ZMDB) | Undocumented binary format; needs study of device contents | High |
| Device internet pass-through | Optional; out of scope for a first version | — |
| Marketplace / Social / sign-in | Services are gone. Replace with local library + optional open catalogs (MusicBrainz, Cover Art Archive, podcast RSS) | — |

## Suggested phase plan
1. **Player first:** library scan into SQLite, GStreamer playback with gapless, Now Playing / Quickplay / Collection views, `.zpl` and `.m3u` import. No device needed.
2. **Podcasts and art:** RSS podcasts and MusicBrainz/Cover Art Archive metadata. This covers most of what the `FeaturesOverride` flags turn on, without needing Microsoft's servers.
3. **DLNA sharing:** expose the library over standard UPnP ContentDirectory.
4. **Device research:** characterise MTPZ and ZMDB from public sources and community knowledge, by observing the USB traffic of a real device, not from Microsoft code.

## Open questions
- Is any MTPZ approach available that doesn't rely on distributing Microsoft's key material? This needs a legal check before anything is shipped.
- What exactly does `resources.zune.net` serve (format and schema)? You can learn this by observing the community server's responses, which is a clean source.
- ZMDB layout per device family.

## Rules for this repo
- Write code from these docs, public specs and observed behaviour only.
- Never commit Microsoft binaries, extracted resources or decompiler output.
- Suggested license for Hop-tunes code: GPL-3.0 (compatible with GStreamer/libmtp usage) or MIT/Apache-2.0 if you want permissive.
