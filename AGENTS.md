# Kernel port instructions

Read `../../AGENTS.md` and `../../notes/README.md` before working here.
This is the Linux 4.19.127 TB-X306F donor repository. The target is the
TB-X606F on Android 10, currently running Linux 4.9.190+.

Work on `x606f-android10-port`. Preserve `x306f-stock` and
`x306f-s220039-source` as the original source baseline. The supplied `out/`
directory contains historical donor build output and is ignored; use
`../../scripts/build-kernel.sh` through `../../scripts/container.sh` to
build into the workspace `out/` instead.

The default `amar_row_wifi_defconfig` builds the donor. Target panel/touch,
DT, config, and module interfaces still require porting. Document evidence
and validation in `../../notes/`. Do not flash or reboot the attached
tablet without the user's authorization for that action.
