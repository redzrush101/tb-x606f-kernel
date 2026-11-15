# Lenovo Tab M10 FHD Plus (TB-X606F) kernel

Linux 4.19.127 for the TB-X606F on Android 10, based on TB-X306F sources.
This build has booted Android on a TB-X606F with S301068 firmware: ADB confirmed
boot completion, the new kernel version, and a running display service.
Full hardware functionality is still being tested.

You need a Linux x86-64 PC, Git, and Podman or Docker. The stock boot template
is included, so you do not need to download the firmware.

```sh
git clone --branch x606f-android10-port https://github.com/redzrush101/tb-x606f-kernel.git
cd tb-x606f-kernel
./build-boot.sh
```

This produces `out-container/boot.img` and `out-container/dtbo.img`.
**Use both images together.**

**[Build and flash instructions](BUILD-BOOT.md)**

The original upstream kernel README is available in [README](README).
