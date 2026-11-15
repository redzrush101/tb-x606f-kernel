# Build a boot.img for the Lenovo TB-X606F

You need a Linux PC, Git, and **either Podman or Docker**. The script downloads
and runs the compiler in a container, so you do not need to install a cross
compiler yourself. Have about 10 GB of free disk space and 8 GB of RAM available.

**This kernel is still an unfinished port.** It builds, but the resulting image
has not been tested on the tablet. Building it successfully does not mean it
will boot.

## 1. Get the source

Open a terminal and run:

```sh
git clone --branch x606f-android10-port https://github.com/redzrush101/tb-x606f-kernel.git
cd tb-x606f-kernel
```

The build scripts are included. You do not need a separate build-kit download.

## 2. Have the stock firmware ready

Use your stock `TB-X606F_S301068_230919_BMP_SVC.zip`. You can leave it zipped.
An unmodified `boot.img` extracted from that firmware also works.

The script needs this file so it can keep the stock ramdisk, device trees and
boot settings while replacing the kernel.

## 3. Start the build

If the firmware ZIP is in your Downloads folder, run:

```sh
./build-boot.sh "$HOME/Downloads/TB-X606F_S301068_230919_BMP_SVC.zip"
```

If it is somewhere else, replace the path with its actual location. For an
extracted stock image, use:

```sh
./build-boot.sh /path/to/stock/boot.img
```

The script chooses Podman if installed, otherwise Docker. The first run takes
longer because it downloads the build tools. Leave the terminal open until you
see `Built /src/out-container/boot.img`.

## 4. Find your image

Your new image is here, inside the cloned repository:

```text
out-container/boot.img
```

The script checks that the image keeps the stock load addresses, ramdisk, both
device-tree copies and 32 MB partition size. It also checks the new image's
hashes. The other files in `out-container/` record the settings and build log.

The image is **unsigned**: changing the kernel invalidates Lenovo's signature.
It cannot be assumed to work with a locked bootloader. The script only builds
files; it does not flash anything onto your tablet.

## If something goes wrong

- **Podman or Docker is missing:** install either one using your Linux
  distribution's package manager, then rerun the command.
- **Docker cannot connect or says permission denied:** make sure Docker is
  running and your user can run `docker info`, or use Podman.
- **The PC runs out of memory:** use fewer compiler jobs:

  ```sh
  JOBS=2 ./build-boot.sh "$HOME/Downloads/TB-X606F_S301068_230919_BMP_SVC.zip"
  ```

- **You want to choose Docker explicitly:**

  ```sh
  ENGINE=docker ./build-boot.sh "$HOME/Downloads/TB-X606F_S301068_230919_BMP_SVC.zip"
  ```

- **Compilation fails:** read `out-container/build.log`. If asking for help,
  include the first error and the lines around it.

For the exact stock offsets, compiler information and known compatibility
limits, see [the technical details](scripts/container-build/DETAILS.md).
