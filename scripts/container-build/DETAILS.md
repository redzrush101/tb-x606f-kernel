# TB-X606F build details

The everyday commands are in [BUILD-BOOT.md](../../BUILD-BOOT.md).

## Hardware result

On 2026-11-15, the TB-X606F booted the `achilles6_row_wifi_defconfig` build on
S301068 Android 10 firmware. Both boot and DTBO were flashed with MTKClient and
read back; each partition matched its build image byte for byte.

ADB then reported:

```text
Linux 4.19.127-x606f-scp2-gb6579c79b-dirty aarch64
sys.boot_completed=1
init.svc.bootanim=stopped
init.svc.surfaceflinger=running
```

The display service reported the internal 1200×1920 display ON. The crash log
buffer was empty at the time of inspection. This establishes Android boot on
that tablet, not complete validation of cameras, sound, sensors, suspend,
networking or every other hardware function.

## What the build produces

The default uses `achilles6_row_wifi_defconfig`, the compiled MT6765 base DTB,
and the `achilles6_row_wifi` board overlay. `fdtoverlay` checks that they merge
before packaging. Output is `out-container/boot.img` (32 MiB) and `dtbo.img`
(8 MiB). They must be flashed together.

`boot-template/stock-boot.img.gz` supplies the original ramdisk and boot header.
It is the stock image from `TB-X606F_S301068_230919_BMP_SVC.zip`, not a backup of
user storage. Its uncompressed SHA-256 is:

```text
aef0b80c7f867c492718db9f3b549f23ff182d3bd816bcaea059f0b511e4bafc
```

The kernel and both boot-image DTB representations are replaced with the
compiled target's kernel and base tree. The board overlay goes in `dtbo.img`.
The ramdisk, board-name field, command line, OS fields and load addresses remain
stock. Kernel size, DTB size, section file positions, boot ID and hashes are
recalculated. Both images get internally verified unsigned AVB hash footers;
Lenovo's signatures cannot remain valid after changing their contents. The
tested bootloader was unlocked and allowed these images. No vbmeta changes
were needed in this test.

| Stock parameter retained | Value |
| --- | --- |
| Header version / size | 2 / 1660 bytes |
| Page size | 2048 bytes |
| Kernel load address | 0x40080000 |
| Ramdisk load address | 0x51b00000 |
| Second load address | 0x40f00000 |
| Tags and DTB load address | 0x47880000 |
| Header OS version / patch level | 10.0.0 / 2023-08 |
| Command line | `bootopt=64S3,32N2,64N2 buildvariant=user` |

With base 0x40000000, mkbootimg offsets are kernel 0x80000, ramdisk 0x11b00000,
second 0xf00000, tags and DTB 0x7880000. The scatter boot partition address
0x1e500000 is a storage offset, separate from the load addresses.

The stock AVB properties say Android 10 / 2019-06-06, while the header says
2023-08. The packer preserves this existing difference. It writes `boot.json`
with input/output hashes, header fields and kernel version. Other records are
`kernel.config`, `source-commit.txt`, `source-changes.patch`, `toolchain.txt` and
`build.log`. Build output stays under the ignored `out-container/` directory.

## Compatibility fixes

The earlier donor-config build with stock 4.9 device trees stalled during SCP
initialization because its DTB lacked the 4.19 driver's `dvfs-opp` property.
The target base tree supplies that binding. SCP initialization now checks its
return values and waits at most three seconds, sleeping between checks; it
previously busy-waited forever and flooded the crash log. The OPP allocation
check and invalid-table error handling are also corrected.

The target overlay selects the existing legacy VCOREFS implementation. The
optional newer-service `dvfsrc-up` driver declines this parent, and the debug
voltage table uses the parent's legacy calibrated values instead of rejected
0x506 firmware calls. The debug voltage checker also rejects an out-of-range
gear index correctly.

The board selects the Android 10 EMI MPU transport. MPU SET/CLEAR/READ/SLVERR
operations use separate firmware IDs 0x260–0x263 with legacy argument/register
packing. Modern boards retain the multiplexed service. The reference ABI is:

https://github.com/mediatek-android-development/android_kernel_mediatek_mt6761-62-4.9/blob/100227b164b750408d61e40e153ec10c49d48863/drivers/misc/mediatek/include/mt-plat/mtk_secure_api.h

`test-x606f-compat.py` compiles the actual C helper functions with mocked
firmware calls and ticks, checking legacy arguments, modern fallback and SCP
success/timeout behavior.

The target overlay no longer references a local sensor node before the fragment
creating it. The generic capacitive sensor is disabled at its definition so it
does not compete with the SX9311 at the same I2C address.

A root-level ramoops device uses the memory already reserved by LK at
0x47c90000 (size 0xe0000). It must not be added under `/reserved-memory`: LK
reserves this address itself and halts on duplicate reserved-memory entries.
The logger uses record size 0x40000, console size 0x40000 and pmsg size 0x10000.

## Toolchain and provenance

The container uses Debian Bullseye's Clang/LLVM 11, LLD 11 and GNU ARM64/ARM32
cross tools from Debian's archive. APT checks signed package indexes and package
hashes. Host utilities use `-fcommon`. Three dock-charger function definitions
have explicit `void` parameter lists to satisfy Clang's prototype checks.
Package/tool versions are recorded; builds are not guaranteed bit-for-bit
identical across hosts.

The vendored `avbtool.py` is the unmodified MIT-licensed AOSP tool, including its
license notice, from:

https://android.googlesource.com/platform/external/avb/+/refs/tags/android-13.0.0_r1/avbtool.py

SHA-256:
`69783733ce5e198317b02a5567cc356e898c891de872f58a963e9d5c082973c6`.

A custom stock boot image or firmware ZIP can still be passed as an optional
argument. The developer-only `BOOT_DTB=stock` option retains the old stock-DTB
packaging for comparisons; that combination is not the boot-tested target.
