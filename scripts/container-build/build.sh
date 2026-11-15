#!/usr/bin/env bash
set -euo pipefail
cd /src
out=/src/out-container
mkdir -p "$out"
exec > >(tee "$out/build.log") 2>&1
[[ ${JOBS:-8} =~ ^[1-9][0-9]*$ ]] || { echo 'JOBS must be a positive integer'; exit 1; }
export KBUILD_BUILD_USER=builder KBUILD_BUILD_HOST=container
export KBUILD_BUILD_VERSION=1
export KBUILD_BUILD_TIMESTAMP="$(git show -s --format=%cD HEAD)"
export KCONFIG_NOTIMESTAMP=1
args=(O="$out/kernel" ARCH=arm64 CC=clang LD=ld.lld
    CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu-
    CROSS_COMPILE_ARM32=arm-linux-gnueabi- CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
    NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump
    HOSTCFLAGS=-fcommon)
make "${args[@]}" "${DEFCONFIG:-amar_row_wifi_defconfig}"
make "${args[@]}" -j"${JOBS:-8}" Image.gz
python3 -B scripts/container-build/repack.py /input/stock \
    "$out/kernel/arch/arm64/boot/Image.gz" "$out/boot.img"
cp "$out/kernel/.config" "$out/kernel.config"
git rev-parse HEAD > "$out/source-commit.txt"
git diff --binary HEAD > "$out/source-changes.patch"
{
    clang --version
    ld.lld --version
    dpkg-query -W clang-11 lld-11 gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi
} > "$out/toolchain.txt"
echo "Built $out/boot.img (see boot.json for the verified stock layout)."
