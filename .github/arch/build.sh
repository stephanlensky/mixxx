#!/usr/bin/env bash
set -euo pipefail

source_dir=/src
build_dir=/build
out_dir=/src/out

trap 'ccache -s || true; chmod -R a+rwX "$out_dir" "$CCACHE_DIR" 2>/dev/null || true' EXIT

pacman -Syu --noconfirm --needed ccache git
(
    source "$source_dir/.github/arch/PKGBUILD"
    pacman -S --noconfirm --needed "${depends[@]}" "${makedepends[@]}"
)

sed -i 's/^OPTIONS=(\(.*\) lto)/OPTIONS=(\1 !lto)/' /etc/makepkg.conf
grep -q '^OPTIONS=.* !lto)' /etc/makepkg.conf
git config --system --add safe.directory '*'

useradd -m builder
mkdir -p "$build_dir" "$out_dir" "$CCACHE_DIR"
cp "$source_dir/.github/arch/PKGBUILD" "$build_dir/"
chown -R builder "$build_dir" "$CCACHE_DIR"
sudo -u builder --preserve-env=CCACHE_DIR ccache -z

cd "$build_dir"
sudo -u builder \
    --preserve-env=CCACHE_DIR,CCACHE_MAXSIZE,MIXXX_SOURCE,QT_QPA_PLATFORM \
    makepkg -f --noconfirm

cp mixxx-git-r*-x86_64.pkg.tar.zst "$out_dir/mixxx-git-x86_64.pkg.tar.zst"
cp mixxx-git-debug-r*-x86_64.pkg.tar.zst "$out_dir/mixxx-git-debug-x86_64.pkg.tar.zst"
ls mixxx-git-r*-x86_64.pkg.tar.zst | sed -E 's/mixxx-git-(r[0-9]+-[0-9]+)-x86_64.pkg.tar.zst/\1/' \
        > "$out_dir/version"
