# Stock boot layout and build details

For the build commands, see [BUILD-BOOT.md](../../BUILD-BOOT.md).

## What matches the supplied stock firmware

By default, the build uses `boot-template/stock-boot.img.gz`, included in this
repository. No separate firmware download is needed. The packer reads the
actual stock header; it does not guess mkbootimg defaults.
For `TB-X606F_S301068_230919_BMP_SVC.zip`, the extracted stock boot SHA-256 is
`aef0b80c7f867c492718db9f3b549f23ff182d3bd816bcaea059f0b511e4bafc`.

| Parameter | Stock value retained |
| --- | --- |
| Header version / size | 2 / 1660 bytes |
| Page size | 2048 bytes |
| Partition size | 0x02000000 (32 MiB) |
| Kernel load address | 0x40080000 |
| Ramdisk load address | 0x51b00000 |
| Second load address | 0x40f00000 |
| Tags address | 0x47880000 |
| DTB load address | 0x47880000 |
| OS version / header patch level | 10.0.0 / 2023-08 |
| Command line | `bootopt=64S3,32N2,64N2 buildvariant=user` |

Using the conventional base `0x40000000`, the corresponding mkbootimg offsets
are kernel `0x00080000`, ramdisk `0x11b00000`, second `0x00f00000`, tags and DTB
`0x07880000`. The scatter file's boot partition starts at `0x1e500000` in eMMC;
that is a storage location, separate from these load addresses.

The ramdisk, appended stock DTB, header-v2 DTB table, board name, extra command
line and OS fields are retained. The stock DTB table is 101811 bytes; the raw
DTB appended to the kernel is 101747 bytes. The 64-byte DT table wrapper is
preserved. The second-stage and recovery-DTBO sections are empty in this stock
image. Kernel size, boot ID, section file positions and AVB hashes are recalculated
for the new kernel. File positions necessarily move when the kernel size changes;
the load addresses above stay unchanged. No separate `dtbo.img` is modified.

AVB hash metadata is regenerated with algorithm `NONE` (unsigned) and the
stock salt, properties and rollback fields. Lenovo's signing key is unavailable,
so Lenovo's RSA signature cannot remain valid after changing the kernel. The
stock AVB boot properties say Android 10 / `2019-06-06`, even though its boot
header says `2023-08`; the script retains that existing difference.

The packer checks preserved components and header parameters and runs
`avbtool verify_image` on the result. Successful AVB hash verification here means
the unsigned image is internally consistent; it does not establish Lenovo trust
or compatibility with a locked bootloader's stock vbmeta chain.

## Hardware compatibility

This repository's `x606f-android10-port` branch is an unfinished Linux 4.19.127
port based on TB-X306F sources. The supplied TB-X606F firmware uses 4.9.190+.
Matching the boot container does not finish that port. In particular, panel,
touch, secure firmware, DT bindings and vendor module interfaces still need
hardware validation. Stock 4.9 modules cannot be assumed to work with 4.19.

This build intentionally retains the stock 4.9 device trees to satisfy the stock
packaging request. It does not include the repository's changed 4.19 device tree
in `boot.img`. Those older trees may also be incompatible with the new kernel.
Treat the result as an experimental build artifact, not a proven booting image.
The scripts do not flash a device, unlock a bootloader, or alter vbmeta.

## Build implementation and provenance

The container uses Debian Bullseye's LLVM/Clang 11, LLD 11 and GNU ARM64/ARM32
cross tools. Bullseye's repositories are read from Debian's archive. `HOSTCFLAGS`
includes `-fcommon` for old host utilities. APT verifies repository signatures and
package checksums. Package versions are recorded by the container build log;
this recipe is not a guarantee of bit-for-bit reproducibility across hosts.

The source includes three compiler fixes: explicit `void` parameter lists for
`dock_charger()` in `rt_pd_manager.c`, `dock_chargered()` in
`mt6357-charger-type.c`, and `check_dock_charger()` in `mtk_charger.c`. These fix
Clang's fatal strict-prototype diagnostics. Any local tracked changes are
recorded in `out-container/source-changes.patch`.

`scripts/container-build/avbtool.py` is the MIT-licensed AOSP tool (license notice included), vendored without
changes from:

https://android.googlesource.com/platform/external/avb/+/refs/tags/android-13.0.0_r1/avbtool.py

Its SHA-256 is
`69783733ce5e198317b02a5567cc356e898c891de872f58a963e9d5c082973c6`.
