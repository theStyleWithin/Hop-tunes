# Submitting Hop-tunes @VERSION@ to Flathub

`io.github.thestylewithin.Hop-tunes.yml` builds Hop-tunes @VERSION@ from the release source
tarball (`hop-tunes-@VERSION@.tar.gz`, sha256 `@SHA256@`) on the KDE 6.11 runtime. Hop-tunes'
release pipeline built this exact manifest with `flatpak-builder`, ran the app inside the
sandbox, and checked the manifest and the build with `flatpak-builder-lint` before
publishing this kit.

## Before you start

- Read https://docs.flathub.org/docs/for-app-authors/submission and the requirements page.
- The app ID `io.github.thestylewithin.Hop-tunes` follows Flathub's rule for GitHub projects:
  it is `github.com/theStyleWithin/Hop-tunes` written as an ID (flatpak-builder-lint checks
  that this address exists). In your submission PR, link the upstream repository
  (https://github.com/theStyleWithin/Hop-tunes) and say you are submitting with upstream's
  agreement; the upstream author can confirm in the PR. Verification on Flathub can be
  added by upstream later.

## Test locally

```sh
flatpak install -y flathub org.flatpak.Builder org.kde.Sdk//6.11 org.kde.Platform//6.11
flatpak run org.flatpak.Builder --user --install --force-clean --install-deps-from=flathub \
    build-dir io.github.thestylewithin.Hop-tunes.yml
flatpak run io.github.thestylewithin.Hop-tunes
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest io.github.thestylewithin.Hop-tunes.yml
```

## Submit

1. Fork https://github.com/flathub/flathub and create a branch from `new-pr`.
2. Add `io.github.thestylewithin.Hop-tunes.yml` at the top level of that branch.
3. Open a pull request against `new-pr` titled "Add io.github.thestylewithin.Hop-tunes".
4. Reply to the reviewers; Flathub's bot builds it (comment `bot, build` to rebuild).

Once accepted, Flathub creates the `flathub/io.github.thestylewithin.Hop-tunes` repository.
For each new Hop-tunes release, replace the `url` and `sha256` of the `hop-tunes` module
with the ones from that release's kit (or let Flathub's external data checker do it).

## Notes

- `COMPATIBILITY.md` (in this kit) lists the distros Hop-tunes is tested on, with CachyOS first, and explains that distros from before 2022 may not work with the AppImage. Feel free to link it from the package page.

- Permissions: Wayland with X11 fallback, GPU, sound (PulseAudio/PipeWire), the Music folder
  read-only, and the MPRIS name for media keys. Other music folders are added through the
  file chooser (document portal), so no home-folder access is needed.
- AAC, WMA and similar codecs come from the runtime's `codecs-extra` extension.
- Hop-tunes is independent and not affiliated with Microsoft. Please don't use Zune names or
  logos in the Flathub listing.
