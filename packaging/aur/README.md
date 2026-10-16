# AUR packages

| Package | Builds |
|---|---|
| `hop-tunes` | the latest release tag (`v$pkgver`) |
| `hop-tunes-git` | the latest commit on `main` |

Both build from source with CMake and Ninja against the system Qt 6, GStreamer, TagLib and
SQLite, run the library/profile/playlist tests in `check()`, and install the binary, desktop
entry, icon, AppStream metadata, man page and license.

## Releasing a new version (`hop-tunes`)

1. Tag the release on GitHub (`v1.0.0`), so the source tarball exists.
2. In `hop-tunes/`: set `pkgver`, reset `pkgrel=1`, then `updpkgsums` (fills `sha256sums`,
   currently `SKIP` until the tag exists) and `makepkg --printsrcinfo > .SRCINFO`.
3. `makepkg -si` to test, `namcap PKGBUILD *.pkg.tar.zst` to lint.
4. Push the two files to `ssh://aur@aur.archlinux.org/hop-tunes.git`.

`hop-tunes-git` only needs a new push when its dependencies or build steps change.

The `aur` job in `.github/workflows/build.yml` runs real `makepkg` and `namcap` in an Arch
Linux container on every push, using the checked-out code as the source tarball.
