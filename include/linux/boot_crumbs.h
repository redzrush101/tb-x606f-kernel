/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_BOOT_CRUMBS_H
#define _LINUX_BOOT_CRUMBS_H

#include <linux/types.h>

#define BOOT_CRUMB_INITCALL	15

#ifdef CONFIG_ARM64_BOOT_CRUMBS
void boot_crumb(unsigned int idx, u64 val);
#define boot_crumb_mark(idx)	boot_crumb(idx, 0xc0deULL | ((u64)(idx) << 16))
#else
static inline void boot_crumb(unsigned int idx, u64 val) { }
#define boot_crumb_mark(idx)	do { } while (0)
#endif

#endif
