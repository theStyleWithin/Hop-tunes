# Releasing Hop-tunes

A release is one git tag. Everything else is automatic and checked before it is published.

## Cut a release

1. Make sure `main` is green (build, appimage, flatpak and release-kit checks).
2. Set the version in `CMakeLists.txt` (`project(hop-tunes VERSION x.y.z ...)`), add a
   `## [x.y.z] - YYYY-MM-DD` section to `CHANGELOG.md` and a `<release>` entry to
   `packaging/io.github.thestylewithin.Hop_tunes.metainfo.xml`. Merge that.
3. Tag and push: `git tag vx.y.z && git push origin vx.y.z`.

## What the tag produces

| Workflow | Checks first | Then attaches to the `vx.y.z` release |
|---|---|---|
| appimage | builds on glibc 2.35; runs the app on 18 distro/display setups | `Hop-tunes-x.y.z-x86_64.AppImage`, `.zsync`, `SHA256SUMS`; release notes from CHANGELOG.md |
| flatpak | builds on the KDE 6.11 runtime; runs the app in the sandbox | `Hop-tunes-x86_64.flatpak` |
| release-kit | real `makepkg` + `namcap` + install on Arch; real `flatpak-builder` build + `flatpak-builder-lint` | `hop-tunes-x.y.z.tar.gz` (source), `hop-tunes-x.y.z-aur-kit.tar.gz`, `hop-tunes-x.y.z-flathub-kit.tar.gz`, `KIT-SHA256SUMS` |

If any check fails, that workflow attaches nothing; fix the problem, delete the tag and tag again.
A tag whose version differs from `CMakeLists.txt` is refused.

## Hand-off kits (for AUR and Flathub publishers)

Hop-tunes' author does not publish on the AUR or Flathub personally. The kits let anyone do
it without touching the code:

- **AUR kit**: `hop-tunes/` and `hop-tunes-git/` with `PKGBUILD` and `.SRCINFO`, the
  checksum of the release source tarball filled in, and `SUBMIT-AUR.md` with step-by-step
  instructions.
- **Flathub kit**: the Flathub manifest building the release source tarball (url + sha256),
  and `SUBMIT-FLATHUB.md`.

The kits are reproducible: `packaging/release-kit/make-kit.sh x.y.z . out/` on the tagged
commit produces byte-identical files.
