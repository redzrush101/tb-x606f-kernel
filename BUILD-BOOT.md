# Build and flash the TB-X606F kernel

You need a Linux x86-64 PC, Git, and **Podman or Docker**. Have about 10 GB of
free disk space and 8 GB of RAM. The compiler runs in a container; the script
sets it up for you. The stock boot template is included—no firmware download
needed.

This version has booted Android on a Lenovo TB-X606F running the S301068 Android
10 firmware. Wi-Fi connected to a saved 5 GHz network and passed an internet
ping test. Bluetooth switched on successfully. GPS and the remaining hardware
still need functional testing.

The vendor drivers are included and built into the kernel. You do not need to
download the X306F source archive or install separate module files on the tablet.

## Build it

Open a terminal and run:

```sh
git clone --branch x606f-android10-port https://github.com/redzrush101/tb-x606f-kernel.git
cd tb-x606f-kernel
./build-boot.sh
```

The first run downloads the build tools and takes longer. When it finishes,
you will find these two files:

```text
out-container/boot.img
out-container/dtbo.img
```

Keep them together. The new kernel needs its matching device trees, which is
why flashing only `boot.img` is not enough.

## Flash it

Your bootloader must already be unlocked and allow modified images. Keep a
backup of your original boot and DTBO partitions. These instructions do not
unlock the device or erase your data.

If you use **MTKClient**, connect the tablet in the same preloader/BROM mode
you normally use. Run these commands from the cloned repository:

```sh
mtk r boot,dtbo stock-boot-backup.img,stock-dtbo-backup.img
mtk w boot,dtbo out-container/boot.img,out-container/dtbo.img
```

After both writes finish, reboot with the tablet's buttons. If the long Power
hold switches it off, release the buttons and press Power again to start it.
`mtk reset` did not reliably start Android on the tested tablet.

If you already use **fastboot** for flashing on your tablet, use:

```sh
fastboot flash boot out-container/boot.img
fastboot flash dtbo out-container/dtbo.img
fastboot reboot
```

Use either method. You do not need to run both sets of commands.

## If it fails to boot

With MTKClient, reconnect in preloader/BROM mode and save a log dump:

```sh
mtk da peek 0x47c90000 0xe0000 --filename pstore-failed-boot.bin
mtk r expdb expdb-failed-boot.bin
```

To restore the backups you made before flashing:

```sh
mtk w boot,dtbo stock-boot-backup.img,stock-dtbo-backup.img
```

Then reboot with the buttons. If asking for help, include both dumps and say
which build you used.

## Build problems

- **Podman or Docker missing:** install either one using your Linux package
  manager. Docker must be running and usable by your account.
- **Not enough memory:** run `JOBS=2 ./build-boot.sh`.
- **Choose Docker explicitly:** run `ENGINE=docker ./build-boot.sh`.
- **Compiler error:** check `out-container/build.log` and share the first error
  with the lines around it.

[Technical details and boot-test evidence](scripts/container-build/DETAILS.md)
