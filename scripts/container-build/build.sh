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
make "${args[@]}" "${DEFCONFIG:-achilles6_row_wifi_defconfig}"
if [[ ${BOOT_DTB:-achilles6_row_wifi} == achilles6_row_wifi ]]; then
    make "${args[@]}" -j"${JOBS:-8}" Image.gz \
        mediatek/mt6765.dtb mediatek/achilles6_row_wifi.dtb
    fdtoverlay -i "$out/kernel/arch/arm64/boot/dts/mediatek/mt6765.dtb" \
        -o "$out/merged-device-tree.dtb" \
        "$out/kernel/arch/arm64/boot/dts/mediatek/achilles6_row_wifi.dtb"
    python3 -B scripts/container-build/repack.py /input/stock \
        "$out/kernel/arch/arm64/boot/Image.gz" "$out/boot.img" \
        "$out/kernel/arch/arm64/boot/dts/mediatek/mt6765.dtb"
    python3 -B scripts/container-build/pack-dtbo.py \
        "$out/kernel/arch/arm64/boot/dts/mediatek/achilles6_row_wifi.dtb" \
        "$out/dtbo.img"
    image="$out/boot.img"
elif [[ ${BOOT_DTB:-achilles6_row_wifi} == stock ]]; then
    make "${args[@]}" -j"${JOBS:-8}" Image.gz
    python3 -B scripts/container-build/repack.py /input/stock \
        "$out/kernel/arch/arm64/boot/Image.gz" "$out/boot.img"
    image="$out/boot.img"
else
    echo "Unknown BOOT_DTB: $BOOT_DTB" >&2
    exit 1
fi
cp "$out/kernel/.config" "$out/kernel.config"
git rev-parse HEAD > "$out/source-commit.txt"
git diff --binary HEAD > "$out/source-changes.patch"
{
    clang --version
    ld.lld --version
    dpkg-query -W clang-11 lld-11 gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi
} > "$out/toolchain.txt"
echo "Built $image (see the matching JSON file for verified boot parameters)."
