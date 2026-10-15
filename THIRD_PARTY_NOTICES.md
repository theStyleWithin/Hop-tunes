# Third-party notices

Hop-tunes is licensed under the GNU General Public License v3.0 or later (see `LICENSE`).
It uses, and the AppImage and Flatpak builds include, the following open-source software.
All are used unmodified, as shared libraries or bundled assets.

| Component | License | Use | Source |
|---|---|---|---|
| Qt 6 (Core, Gui, Qml, Quick, Quick Controls 2, DBus, Wayland and XCB platform plugins) | LGPL-3.0 | user interface | https://code.qt.io |
| GStreamer 1.x and gst-plugins-base / good / ugly, gst-libav | LGPL-2.1-or-later | audio decoding and playback | https://gstreamer.freedesktop.org |
| FFmpeg (through gst-libav) | LGPL-2.1-or-later | AAC, WMA and other decoders | https://ffmpeg.org |
| TagLib | LGPL-2.1 / MPL-1.1 | reading tags and cover art | https://taglib.org |
| SQLite | Public domain | track index and profile data | https://sqlite.org |
| GLib / GIO | LGPL-2.1-or-later | MPRIS over D-Bus | https://gitlab.gnome.org/GNOME/glib |
| Jost font (300, 400, 500) | SIL Open Font License 1.1 (`assets/fonts/OFL.txt`) | interface font | https://github.com/indestructible-type/Jost |

The AppImage also contains the shared libraries these depend on (for example libpng,
libjpeg, FreeType, HarfBuzz, libxcb). Each keeps its own license; the AppImage's
`usr/share/doc` folder, where present, carries their copyright files. LGPL libraries are
dynamically linked, so they can be replaced: extract the AppImage with
`--appimage-extract` and swap the `.so` files in `usr/lib`.

## Trademarks

Zune and Microsoft are trademarks of Microsoft Corporation. Hop-tunes is an independent,
clean-room project inspired by how the Zune software organised music. It contains no
Microsoft code, artwork, fonts or other assets and is not affiliated with or endorsed by
Microsoft.
