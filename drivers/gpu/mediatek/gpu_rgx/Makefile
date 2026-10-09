#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

$(info [GPU_DEBUG] MTK_GPU_VERSION 2 = $(word 2,$(MTK_GPU_VERSION)))

# The kernel driver must match the DDK of the userspace blobs; honour an
# explicit DDK directory (third word, e.g. "rgx doma m1.11ED5425693").
ddk := $(word 3,$(MTK_GPU_VERSION))
ifneq ($(ddk),)
ifneq ($(wildcard $(srctree)/$(src)/$(ddk)/Makefile),)
	obj-y += $(ddk)/
else
$(error MTK_GPU_VERSION names missing DDK $(ddk))
endif
else ifeq ($(word 2,$(MTK_GPU_VERSION)),doma)
	obj-y += m1.13ED5776728/
else ifeq ($(word 2,$(MTK_GPU_VERSION)),lorne)
	obj-y += m1.13ED5776728/
else ifeq ($(word 2,$(MTK_GPU_VERSION)),clark)
	obj-y += m1.13ED5776728/
endif

