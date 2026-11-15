# Bundled stock boot template

`stock-boot.img.gz` is the original `boot.img` from the Lenovo firmware
`TB-X606F_S301068_230919_BMP_SVC.zip`, compressed with gzip. It comes from the
stock service firmware, not a backup of a user's tablet, and contains no user
data. You do not need the firmware ZIP to build.

The build script uses it to preserve the ramdisk, device trees, load addresses
and boot header settings. Its original kernel is replaced with the kernel
compiled from this repository. AVB hash metadata is regenerated unsigned.

SHA-256 of the uncompressed stock boot image:

```text
aef0b80c7f867c492718db9f3b549f23ff182d3bd816bcaea059f0b511e4bafc
```

The complete firmware ZIP is not included.
