/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SOC_MTK_VCOREFS_H
#define __SOC_MTK_VCOREFS_H

#include <linux/types.h>

int mtk_spm_vcorefs_call(u32 cmd, u32 arg1, u32 arg2);

#endif
