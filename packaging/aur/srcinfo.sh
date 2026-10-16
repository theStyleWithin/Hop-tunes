#!/usr/bin/env bash
# Prints .SRCINFO for a single-package PKGBUILD (used where makepkg is not available).
# Usage: srcinfo.sh PKGBUILD > .SRCINFO
source "$1"
printf 'pkgbase = %s\n' "$pkgname"
printf '\tpkgdesc = %s\n' "$pkgdesc"
printf '\tpkgver = %s\n' "$pkgver"
printf '\tpkgrel = %s\n' "$pkgrel"
printf '\turl = %s\n' "$url"
for a in "${arch[@]}"; do printf '\tarch = %s\n' "$a"; done
for l in "${license[@]}"; do printf '\tlicense = %s\n' "$l"; done
for d in "${checkdepends[@]}"; do printf '\tcheckdepends = %s\n' "$d"; done
for d in "${makedepends[@]}"; do printf '\tmakedepends = %s\n' "$d"; done
for d in "${depends[@]}"; do printf '\tdepends = %s\n' "$d"; done
for d in "${optdepends[@]}"; do printf '\toptdepends = %s\n' "$d"; done
for d in "${provides[@]}"; do printf '\tprovides = %s\n' "$d"; done
for d in "${conflicts[@]}"; do printf '\tconflicts = %s\n' "$d"; done
for s in "${source[@]}"; do printf '\tsource = %s\n' "$s"; done
for s in "${sha256sums[@]}"; do printf '\tsha256sums = %s\n' "$s"; done
printf '\npkgname = %s\n' "$pkgname"
