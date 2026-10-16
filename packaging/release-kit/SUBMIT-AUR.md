# Publishing Hop-tunes @VERSION@ on the AUR

This kit holds two ready packages. Both were built with `makepkg`, checked with `namcap`
and installed on Arch Linux by Hop-tunes' release pipeline before this kit was published.

| Folder | AUR package | Builds |
|---|---|---|
| `hop-tunes/` | `hop-tunes` | release @VERSION@ from `hop-tunes-@VERSION@.tar.gz` (sha256 `@SHA256@`) |
| `hop-tunes-git/` | `hop-tunes-git` | the latest commit on `main` |

You need an AUR account with your SSH key added (https://aur.archlinux.org/account).

## First upload

```sh
# 1. Test the build locally (installs build deps, runs the tests in check(), builds the package)
cd hop-tunes
makepkg -si
namcap PKGBUILD ./*.pkg.tar.zst      # should report no errors
hop-tunes --version                  # prints "hop-tunes @VERSION@"

# 2. Publish: an AUR package is a git repo holding PKGBUILD and .SRCINFO
git clone ssh://aur@aur.archlinux.org/hop-tunes.git aur-hop-tunes
cp PKGBUILD .SRCINFO aur-hop-tunes/
cd aur-hop-tunes
git add PKGBUILD .SRCINFO
git commit -m "Hop-tunes @VERSION@"
git push
```

Do the same for `hop-tunes-git/` with `ssh://aur@aur.archlinux.org/hop-tunes-git.git`.

Before publishing, change the `# Maintainer:` line in each PKGBUILD to your own name and
email, and run `makepkg --printsrcinfo > .SRCINFO` again.

## Later releases

Each new Hop-tunes release has a fresh kit like this one with the new version and checksum;
copy its `hop-tunes/PKGBUILD` and `.SRCINFO` over yours (keeping your Maintainer line),
commit and push. `hop-tunes-git` only needs updating when its dependencies change.

## Notes

- `COMPATIBILITY.md` (in this kit) lists the distros Hop-tunes is tested on, with CachyOS first, and explains that distros from before 2022 may not work with the AppImage. Feel free to link it from the package page.

- Upstream: https://github.com/theStyleWithin/Hop-tunes (GPL-3.0-or-later). Bugs in the app
  go to upstream issues; packaging problems to the AUR comments.
- The package runs only the tests that need no sound card or display in `check()`.
- Hop-tunes is independent and not affiliated with Microsoft. Please don't use Zune names or
  logos in the AUR description.
