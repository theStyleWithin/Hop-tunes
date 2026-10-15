#!/usr/bin/env bash
# Builds the hand-off kits for people who publish Hop-tunes on the AUR and Flathub.
#
#   make-kit.sh <version> <repo-dir> <out-dir> [source-url-base]
#
# Writes to <out-dir>:
#   hop-tunes-<v>.tar.gz              release source tarball (git archive of tag v<v>, or of
#                                     HEAD when the tag does not exist, e.g. in CI tests)
#   hop-tunes-<v>-aur-kit.tar.gz      hop-tunes/ and hop-tunes-git/: PKGBUILD + .SRCINFO,
#                                     checksums filled in, plus SUBMIT-AUR.md
#   hop-tunes-<v>-flathub-kit.tar.gz  io.github.thestylewithin.Hop-tunes.yml pointing at the
#                                     source tarball (url + sha256), plus SUBMIT-FLATHUB.md
#   KIT-SHA256SUMS
# source-url-base defaults to the GitHub release download URL for v<v>.
set -euo pipefail
ver=$1; repo=$(cd "$2" && pwd); out=$3
base=${4:-https://github.com/theStyleWithin/Hop-tunes/releases/download/v$ver}
mkdir -p "$out"; out=$(cd "$out" && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
# Reproducible archives: fixed order, owner and timestamps.
pack() { tar -C "$work" --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf - "$1" | gzip -n -9 > "$2"; }

# 1. Source tarball: reproducible for a given commit (git archive stores the commit time).
ref=HEAD
git -C "$repo" rev-parse -q --verify "refs/tags/v$ver" >/dev/null && ref="v$ver"
src="hop-tunes-$ver.tar.gz"
git -C "$repo" archive --format=tar --prefix="hop-tunes-$ver/" "$ref" | gzip -n -9 > "$out/$src"
sha=$(sha256sum "$out/$src" | cut -d' ' -f1)
echo "source: $src ($ref) sha256=$sha"

# 2. AUR kit.
aur="$work/hop-tunes-$ver-aur-kit"
mkdir -p "$aur"
cp -r "$repo/packaging/aur/hop-tunes" "$repo/packaging/aur/hop-tunes-git" "$aur/"
sed -i "s|^pkgver=.*|pkgver=$ver|; s|^pkgrel=.*|pkgrel=1|; s|^sha256sums=.*|sha256sums=('$sha')|" "$aur/hop-tunes/PKGBUILD"
sed -i "s|^source=.*|source=(\"$base/\$pkgname-\$pkgver.tar.gz\")|" "$aur/hop-tunes/PKGBUILD"
for p in hop-tunes hop-tunes-git; do
  if command -v makepkg >/dev/null; then (cd "$aur/$p" && makepkg --printsrcinfo > .SRCINFO)
  else (cd "$aur/$p" && bash "$repo/packaging/aur/srcinfo.sh" PKGBUILD > .SRCINFO); fi
done
sed "s/@VERSION@/$ver/g; s/@SHA256@/$sha/g" "$repo/packaging/release-kit/SUBMIT-AUR.md" > "$aur/SUBMIT-AUR.md"
pack "hop-tunes-$ver-aur-kit" "$out/hop-tunes-$ver-aur-kit.tar.gz"

# 3. Flathub kit: same manifest as CI, but building the release tarball.
fh="$work/hop-tunes-$ver-flathub-kit"
mkdir -p "$fh"
python3 - "$repo/packaging/flatpak/io.github.thestylewithin.Hop-tunes.yml" "$fh/io.github.thestylewithin.Hop-tunes.yml" "$base/$src" "$sha" <<'PY'
import sys
src, dst, url, sha = sys.argv[1:]
text = open(src).read()
start = text.index("      # CI builds the checked-out tree.")
text = text[:start] + f"      - type: archive\n        url: {url}\n        sha256: {sha}\n"
text = text.replace("# Local build:  flatpak-builder --user --install --force-clean build-dir packaging/flatpak/io.github.thestylewithin.Hop-tunes.yml",
                    "# Local build:  flatpak-builder --user --install --force-clean build-dir io.github.thestylewithin.Hop-tunes.yml")
open(dst, "w").write(text)
PY
sed "s/@VERSION@/$ver/g; s/@SHA256@/$sha/g" "$repo/packaging/release-kit/SUBMIT-FLATHUB.md" > "$fh/SUBMIT-FLATHUB.md"
pack "hop-tunes-$ver-flathub-kit" "$out/hop-tunes-$ver-flathub-kit.tar.gz"

(cd "$out" && sha256sum "$src" "hop-tunes-$ver-aur-kit.tar.gz" "hop-tunes-$ver-flathub-kit.tar.gz" > KIT-SHA256SUMS)
echo "kits written to $out"
