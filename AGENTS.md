# Kernel port instructions

Read `BUILD-BOOT.md` and `scripts/container-build/DETAILS.md` before working here.
This is the Linux 4.19.127 TB-X306F donor repository. The target is the
TB-X606F on Android 10, currently running Linux 4.9.190+.

Work on `x606f-android10-port`. Preserve `x306f-stock` and
`x306f-s220039-source` as the original source baseline. The historical `out/`
directory is ignored. Use `./build-boot.sh` to build with Podman or Docker into
`out-container/`, using the bundled stock boot template by default. A stock
firmware ZIP or boot image may be passed as an optional argument.

The default `amar_row_wifi_defconfig` builds the donor. Target panel/touch,
DT, config, and module interfaces still require porting. Document build and
packaging details in `scripts/container-build/DETAILS.md`. Do not flash or
reboot the attached tablet without the user's authorization for that action.
