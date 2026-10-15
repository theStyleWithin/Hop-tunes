# Which Linux distros Hop-tunes runs on

Hop-tunes is tested automatically on every change. The table below lists every system it
has passed on. A pass means the app started, created a profile, scanned and played a
song, and drew its window.

> [!NOTE]
> **Older distros may not work.** Hop-tunes is built for current Linux. On a distro
> released before 2022 (for example Ubuntu 20.04, Debian 11, RHEL/AlmaLinux/Rocky 9 or
> CentOS 7), the **AppImage** opens but cannot play music, because its audio codecs need
> glibc 2.35 or newer. On those systems, use the **Flatpak**, which brings its own
> libraries. Older systems are not tested, so other problems are possible too.

## ⭐ CachyOS

**Hop-tunes is made on CachyOS:** it's the developer's own desktop, and the AppImage is
tested on CachyOS on every change. To install there:

```sh
paru -S hop-tunes      # or: yay -S hop-tunes
```

The Flatpak and the AppImage work on CachyOS too.

## Tested distros

| Distro | AppImage | Notes |
|---|---|---|
| ⭐ **CachyOS** (latest) | ✅ | the developer's own system; the AUR package is the recommended install |
| Arch Linux (latest) | ✅ | the AUR package is built, checked and installed here on every change |
| Manjaro (latest) | ✅ | |
| Ubuntu 22.04 LTS | ✅ | oldest supported Ubuntu |
| Ubuntu 24.04 LTS | ✅ | also tested on real **X11** and **Wayland** desktop sessions |
| Ubuntu (latest release) | ✅ | |
| Linux Mint 22 | ✅ | |
| Debian 12 | ✅ | oldest supported Debian |
| Debian 13 | ✅ | |
| Debian testing | ✅ | |
| Fedora 41 | ✅ | |
| Fedora 42 | ✅ | |
| openSUSE Tumbleweed | ✅ | |
| openSUSE Leap 15.6 | ✅ | |
| AlmaLinux 10 | ✅ | also covers RHEL 10 and Rocky 10 |
| Void Linux (glibc) | ✅ | the musl edition of Void is not supported by the AppImage; use the Flatpak |

**Not supported by the AppImage:** AlmaLinux / RHEL / Rocky 9 and anything older (tested:
the app opens but plays no sound). Use the Flatpak.

## Which install should I pick?

| You use | Best choice | Why |
|---|---|---|
| CachyOS, Arch, EndeavourOS, Manjaro | **AUR** (`hop-tunes`) | built for your system, updates with the rest of it |
| Any other distro, or an older one | **Flatpak** | brings its own libraries, so it works almost everywhere Flatpak does |
| You don't want to install anything | **AppImage** | one file you run directly; needs a 2022-or-newer distro |

### Flatpak

The Flatpak runs on the KDE Platform runtime and doesn't depend on your distro's
libraries. It works on any x86_64 distro that can run current Flatpak apps, including the
older ones the AppImage can't run on. It is built and run inside its sandbox on every
change.

### AUR

The AUR package builds against your system's own Qt, GStreamer and TagLib. Because Arch
and Arch-based distros update continuously, it is checked against current Arch on every
change: built with `makepkg`, checked with `namcap`, installed, and started. Distros built
on Arch that hold packages back for a long time may need a full system update
(`sudo pacman -Syu`) first.

## Something not working?

Please open an issue at https://github.com/theStyleWithin/Hop-tunes/issues and include:

- your distro and version,
- how you installed Hop-tunes (Flatpak, AUR or AppImage),
- what happened, and the output of `hop-tunes --version`.
