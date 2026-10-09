# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_MEDIATEK) += auto2712p1v1-ivi-boot.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += auto2712p1v1-ivi-nand-boot.dtb


# MediaTek Android projects: main SoC DTB plus project overlay from Kconfig.
MTK_DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES) \
		 $(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES))
dtb-y += $(patsubst mediatek/%,%.dtb,$(filter mediatek/%,$(MTK_DTB_NAMES)))
