# Submitting Hop-tunes @VERSION@ to Flathub

`io.github.thestylewithin.hoptunes.yml` builds Hop-tunes @VERSION@ from the release source
tarball (`hop-tunes-@VERSION@.tar.gz`, sha256 `@SHA256@`) on the KDE 6.8 runtime. Hop-tunes'
release pipeline built this exact manifest with `flatpak-builder`, ran the app inside the
sandbox, and checked the manifest and the build with `flatpak-builder-lint` before
publishing this kit.

## Before you start

- Read https://docs.flathub.org/docs/for-app-authors/submission and the requirements page.
- The app ID `io.github.thestylewithin.hoptunes` is based on the upstream author's GitHub
  account (theStyleWithin). In your submission PR, link the upstream repository
  (https://github.com/theStyleWithin/Hop-tunes) and say you are submitting with upstream's
  agreement; the upstream author can confirm in the PR. Verification on Flathub can be
  added by upstream later.

## Test locally

```sh
flatpak install -y flathub org.flatpak.Builder org.kde.Sdk//6.8 org.kde.Platform//6.8
flatpak run org.flatpak.Builder --user --install --force-clean --install-deps-from=flathub \
    build-dir io.github.thestylewithin.hoptunes.yml
flatpak run io.github.thestylewithin.hoptunes
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest io.github.thestylewithin.hoptunes.yml
```

## Submit

1. Fork https://github.com/flathub/flathub and create a branch from `new-pr`.
2. Add `io.github.thestylewithin.hoptunes.yml` at the top level of that branch.
3. Open a pull request against `new-pr` titled "Add io.github.thestylewithin.hoptunes".
4. Reply to the reviewers; Flathub's bot builds it (comment `bot, build` to rebuild).

Once accepted, Flathub creates the `flathub/io.github.thestylewithin.hoptunes` repository.
For each new Hop-tunes release, replace the `url` and `sha256` of the `hop-tunes` module
with the ones from that release's kit (or let Flathub's external data checker do it).

## Notes

- Permissions: Wayland with X11 fallback, GPU, sound (PulseAudio/PipeWire), the Music folder
  read-only, and the MPRIS name for media keys. Other music folders are added through the
  file chooser (document portal), so no home-folder access is needed.
- AAC, WMA and similar codecs come from the `org.freedesktop.Platform.ffmpeg-full`
  extension, declared in the manifest.
- Hop-tunes is independent and not affiliated with Microsoft. Please don't use Zune names or
  logos in the Flathub listing.
