#!/usr/bin/env bash
# Run from any directory; only Bash and Podman or Docker are needed on the host.
set -euo pipefail
if [[ $# != 1 || $1 == --help || $1 == -h ]]; then
    echo "Usage: $0 /path/to/stock-firmware.zip-or-boot.img"
    echo "Environment: ENGINE=podman|docker JOBS=8 DEFCONFIG=amar_row_wifi_defconfig"
    exit "$(( $# == 1 ? 0 : 2 ))"
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
input=$(realpath -- "$1")
[[ -f $input ]] || { echo "Stock image not found: $input" >&2; exit 1; }
engine=${ENGINE:-}
if [[ -z $engine ]]; then
    if command -v podman >/dev/null; then engine=podman; else engine=docker; fi
fi
command -v "$engine" >/dev/null || { echo 'Install Podman or Docker first.' >&2; exit 1; }
image=localhost/tb-x606f-builder:bullseye
"$engine" build -t "$image" -f "$root/scripts/container-build/Containerfile" \
    "$root/scripts/container-build"
mkdir -p "$root/out-container"
user_args=(--user "$(id -u):$(id -g)")
if [[ $engine == *podman ]]; then user_args=(--userns=keep-id); fi
"$engine" run --rm "${user_args[@]}" --security-opt label=disable \
    -e HOME=/tmp -e JOBS="${JOBS:-8}" \
    -e DEFCONFIG="${DEFCONFIG:-amar_row_wifi_defconfig}" \
    -v "$root:/src:rw" -v "$input:/input/stock:ro" \
    "$image" bash scripts/container-build/build.sh
