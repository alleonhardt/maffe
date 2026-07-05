#!/usr/bin/env bash
set -euo pipefail

build_dir="build-docker-competition"
dist_dir="dist-competition"
targets=(maffe-exact maffe-heuristic maffe-lowerbound)

apt-get update
apt-get install -y --no-install-recommends \
binutils \
build-essential \
ca-certificates \
cmake \
file \
git \
meson \
ninja-build \
patch \
pkg-config
rm -rf /var/lib/apt/lists/*

meson_args=(
  "$build_dir"
  --buildtype=release
  -Db_ndebug=true
  -Db_lto=true
  -Dcompetition=true
  -Dgurobi=disabled
)

if [[ -d "$build_dir/meson-info" ]]; then
  meson setup --reconfigure "${meson_args[@]}"
else
  meson setup "${meson_args[@]}"
fi

meson compile -C "$build_dir" "${targets[@]}"

mkdir -p "$dist_dir"
for target in "${targets[@]}"; do
  strip --strip-all -o "$dist_dir/$target" "$build_dir/$target"
  file "$dist_dir/$target"
  readelf -d "$dist_dir/$target" | grep -q "There is no dynamic section"
  ls -lh "$dist_dir/$target"
done
