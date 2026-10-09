// SPDX-License-Identifier: GPL-2.0
/*
 * Early boot breadcrumbs for bring-up without a UART; see
 * CONFIG_ARM64_BOOT_CRUMBS. Slots 0-5 are written by head.S.
 */
#include <linux/boot_crumbs.h>
#include <linux/cache.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <asm/cacheflush.h>

u64 boot_crumbs[16] __aligned(SMP_CACHE_BYTES) = { [0 ... 15] = 0xdeadbeefULL };

void boot_crumb(unsigned int idx, u64 val)
{
	if (idx >= ARRAY_SIZE(boot_crumbs))
		return;
	WRITE_ONCE(boot_crumbs[idx], val);
	__flush_dcache_area(&boot_crumbs[idx], sizeof(boot_crumbs[idx]));
}

#if CONFIG_ARM64_BOOT_CRUMBS_RESTART_SECS > 0
/*
 * Restart path that does not depend on workqueues or the restart handler
 * chain: a timer on every CPU writes the watchdog reset registers directly
 * (MediaTek TOPRGU: WDT_MODE = KEY | EXTEN | AUTO_RESTART, the
 * "bypass power key" normal reboot, then WDT_SWRST). Falls back to
 * emergency_restart() when no watchdog base is configured.
 */
#define TOPRGU_WDT_MODE		0x00
#define TOPRGU_WDT_SWRST	0x14
#define TOPRGU_MODE_REBOOT	0x22000014
#define TOPRGU_SWRST_KEY	0x1209

static void __iomem *boot_crumbs_wdt;
static struct timer_list boot_crumbs_timer[NR_CPUS];
static atomic_t boot_crumbs_fired = ATOMIC_INIT(0);
static bool boot_crumbs_disarmed;

static void boot_crumbs_restart(struct timer_list *t)
{
	if (READ_ONCE(boot_crumbs_disarmed) ||
	    atomic_xchg(&boot_crumbs_fired, 1))
		return;
	pr_emerg("boot_crumbs: boot not confirmed after %ds, restarting\n",
		 CONFIG_ARM64_BOOT_CRUMBS_RESTART_SECS);
	boot_crumb(14, 0xb007);
	__flush_dcache_area(log_buf_addr_get(), log_buf_len_get());
	if (boot_crumbs_wdt) {
		writel(TOPRGU_MODE_REBOOT, boot_crumbs_wdt + TOPRGU_WDT_MODE);
		writel(TOPRGU_SWRST_KEY, boot_crumbs_wdt + TOPRGU_WDT_SWRST);
		mdelay(1000);
	}
	emergency_restart();
}

static int boot_crumbs_disarm(const char *val, const struct kernel_param *kp)
{
	bool disarm;
	int ret = kstrtobool(val, &disarm), cpu;

	if (ret)
		return ret;
	if (!disarm)
		return 0;
	WRITE_ONCE(boot_crumbs_disarmed, true);
	for_each_possible_cpu(cpu)
		if (boot_crumbs_timer[cpu].function)
			del_timer_sync(&boot_crumbs_timer[cpu]);
	pr_info("boot_crumbs: restart timer disarmed\n");
	return 0;
}

static const struct kernel_param_ops boot_crumbs_disarm_ops = {
	.set = boot_crumbs_disarm,
};
module_param_cb(disarm, &boot_crumbs_disarm_ops, NULL, 0200);

static void boot_crumbs_arm(int cpu)
{
	unsigned long deadline = INITIAL_JIFFIES +
		CONFIG_ARM64_BOOT_CRUMBS_RESTART_SECS * HZ;

	timer_setup(&boot_crumbs_timer[cpu], boot_crumbs_restart, TIMER_PINNED);
	boot_crumbs_timer[cpu].expires = time_after(deadline, jiffies + 1) ?
					 deadline : jiffies + 1;
	add_timer_on(&boot_crumbs_timer[cpu], cpu);
}

static int __init boot_crumbs_restart_early(void)
{
	if (CONFIG_ARM64_BOOT_CRUMBS_WDT_BASE)
		boot_crumbs_wdt = ioremap(CONFIG_ARM64_BOOT_CRUMBS_WDT_BASE, 0x100);
	boot_crumbs_arm(smp_processor_id());
	return 0;
}
early_initcall(boot_crumbs_restart_early);

/* once the secondaries are up, arm a timer on each of them as well */
static int __init boot_crumbs_restart_late(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		if (!boot_crumbs_timer[cpu].function)
			boot_crumbs_arm(cpu);
	return 0;
}
late_initcall(boot_crumbs_restart_late);
#endif
