# Lenovo TB-X306F vendor kernel sources

Imported from Lenovo's TB-X306F S220039 Android source release:

https://download.lenovo.com/consumer/mobiles/tb-x306f_opensource_tb-x306f_s220039_210611_bmp.tar.gz

Archive SHA-256:
`5b39cc5e7b51a247985b073c068b6a2ac56468b804b747b0ca0f02ec737b87ee`.

These are the archive's `vendor/mediatek/kernel_modules` sources. Nested Git
metadata and generated binaries were omitted. Keep the original license notices.

The TB-X606F build selects WMT/common, the AXI Gen4m WLAN driver for MT6765,
legacy Bluetooth, GPS, FM (MT6631) and MET 4.14. Other chip variants remain
available as source but are not linked. MET has no MT6765 platform implementation
in this release; its default stub does not provide hardware instrumentation.

The selected connectivity drivers are built into the kernel. WMT's Android
loader initializes them through its existing ioctl interface, so their deferred
initialization and cleanup functions must remain outside freed init/exit
sections. GPS also registers its EMI and firmware-log interfaces in that path.

Local integration changes select built-in linking, fix Gen4m built-in detection,
pass the configured FM chip to its build,
remove an unused UART lock and give the WLAN firmware pointer internal linkage
to avoid global symbol collisions with other built-in drivers.
