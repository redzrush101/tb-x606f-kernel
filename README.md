# Lenovo Tab M10 FHD Plus (TB-X606F) kernel

This is an unfinished Linux 4.19.127 port for the TB-X606F, based on TB-X306F
sources. The stock Android 10 firmware runs Linux 4.9.190+.

To build a `boot.img`, you need a Linux PC, Git, Podman or Docker, and the stock
firmware ZIP (or its original `boot.img`). The build scripts are included.

**[Follow the build instructions](BUILD-BOOT.md)** — they cover getting the
source, running the build, and finding the finished image.

The image keeps the stock boot layout, ramdisk and device trees. It is unsigned
and has not been boot-tested on the tablet. Hardware support still needs work.

The original upstream kernel README is available in [README](README).
