#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
image="maffe-competition-gcc16-trixie"
dist="$root/dist-competition"
targets=(maffe-exact maffe-heuristic maffe-lowerbound)
no_cache=0

usage() {
  cat <<EOF
Usage: $0 [--no-cache]

Build competition binaries in the GCC 16 trixie container and copy the
stripped outputs to dist-competition/.
EOF
}

while (($#)); do
  case "$1" in
    --no-cache)
      no_cache=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

tmpdir="$(mktemp -d)"
cid=""
cleanup() {
  if [[ -n "$cid" ]]; then
    docker rm "$cid" >/dev/null 2>&1 || true
  fi
  rm -rf "$tmpdir"
}
trap cleanup EXIT

cat > "$tmpdir/Dockerfile" <<'EOF'
FROM gcc:16-trixie AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    binutils \
    ca-certificates \
    cmake \
    file \
    git \
    ninja-build \
    pkg-config \
    meson \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN meson setup /tmp/build /src \
      --buildtype=release \
      -Db_ndebug=true \
      -Db_lto=true \
      -Dcompetition=true \
      -Dgurobi=disabled \
 && meson compile -C /tmp/build maffe-exact maffe-heuristic maffe-lowerbound \
 && mkdir -p /out \
 && for target in maffe-exact maffe-heuristic maffe-lowerbound; do \
      strip --strip-all -o "/out/$target" "/tmp/build/$target"; \
      file "/out/$target"; \
      readelf -d "/out/$target" | grep -q "There is no dynamic section"; \
    done
EOF

docker_args=(build -t "$image" -f "$tmpdir/Dockerfile")
if [[ "$no_cache" == 1 ]]; then
  docker_args+=(--no-cache)
fi
docker_args+=("$root")
docker "${docker_args[@]}"

mkdir -p "$dist"
cid="$(docker create "$image")"
docker cp "$cid:/out/." "$dist/"
docker rm "$cid" >/dev/null
cid=""

for target in "${targets[@]}"; do
  file "$dist/$target"
  readelf -d "$dist/$target" | grep -q "There is no dynamic section"
  ls -lh "$dist/$target"
done
