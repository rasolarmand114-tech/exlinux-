// SPDX-License-Identifier: GPL-2.0
/*
 * arg/arg_core.c — ARG Scheduler Module: init/exit + module glue
 *
 * This is the thin C wrapper that:
 *   1. Calls Rust init/exit
 *   2. Arms/disarms the hooks in kernel/sched/arg_hook.c
 *   3. Exposes module_param knobs (sysfs)
 *   4. Registers CPU hotplug notifier
 *
 * All scheduling logic is in Rust (src/) and Assembly (arch/arm64/).
 *
 * "Linux Scheduler remains the authority.
 *  ARG provides runtime optimization."
 */

#define pr_fmt(fmt) "arg_sched: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include "include/arg_abi.h"

/* ── Symbols from kernel/sched/arg_hook.c ────────────────────── */
extern bool arg_enabled;
extern struct task_struct *(*arg_pick_next_hook)(struct rq *rq);
extern void (*arg_enqueue_hook)(struct rq *rq, struct task_struct *p);
extern void (*arg_wakeup_hook)(struct rq *rq, struct task_struct *p);
extern void (*arg_update_load_hook)(void);

/* ── C hook wrappers declared in hook/*.c ────────────────────── */
struct task_struct *arg_do_pick_next(struct rq *rq);
void arg_do_enqueue(struct rq *rq, struct task_struct *p);
void arg_do_wakeup(struct rq *rq, struct task_struct *p);
void arg_do_update_load(void);
int  arg_cpu_notify(struct notifier_block *nb,
		    unsigned long action, void *hcpu);

/* ── CPU hotplug notifier ─────────────────────────────────────── */
static struct notifier_block arg_cpu_nb = {
	.notifier_call = arg_cpu_notify,
	.priority      = INT_MIN,   /* last — don't race cpufreq */
};

/* ── module_param: async ─────────────────────────────────────── */
static int arg_set_async(const char *val, const struct kernel_param *kp)
{
	int v, ret = kstrtoint(val, 10, &v);
	if (ret || (v != 0 && v != 1)) return ret ?: -EINVAL;
	arg_rust_set_async(v);
	pr_info("async → %d\n", v);
	return 0;
}
static int arg_get_async(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%d\n", arg_rust_get_async()); }
static const struct kernel_param_ops async_ops = { .set=arg_set_async, .get=arg_get_async };
module_param_cb(async, &async_ops, NULL, 0644);
MODULE_PARM_DESC(async, "1=async/gaming (default)  0=sync/throughput");

/* ── module_param: pack ──────────────────────────────────────── */
static int arg_set_pack(const char *val, const struct kernel_param *kp)
{
	int v, ret = kstrtoint(val, 10, &v);
	if (ret) return ret;
	arg_rust_set_pack(!!v);
	pr_info("pack → %d\n", !!v);
	return 0;
}
static int arg_get_pack(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%d\n", arg_rust_get_pack()); }
static const struct kernel_param_ops pack_ops = { .set=arg_set_pack, .get=arg_get_pack };
module_param_cb(pack, &pack_ops, NULL, 0644);
MODULE_PARM_DESC(pack, "1=task packing on (saves energy)  0=off");

/* ── module_param: energy ────────────────────────────────────── */
static int arg_set_energy(const char *val, const struct kernel_param *kp)
{
	int v, ret = kstrtoint(val, 10, &v);
	if (ret) return ret;
	arg_rust_set_energy(!!v);
	pr_info("energy → %d\n", !!v);
	return 0;
}
static int arg_get_energy(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%d\n", arg_rust_get_energy()); }
static const struct kernel_param_ops energy_ops = { .set=arg_set_energy, .get=arg_get_energy };
module_param_cb(energy, &energy_ops, NULL, 0644);
MODULE_PARM_DESC(energy, "1=energy-aware mode  0=off");

/* ── module_param: fps ───────────────────────────────────────── */
static int arg_set_fps(const char *val, const struct kernel_param *kp)
{
	int v, ret = kstrtoint(val, 10, &v);
	if (ret) return ret;
	arg_rust_set_fps(!!v);
	pr_info("fps → %d\n", !!v);
	return 0;
}
static int arg_get_fps(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%d\n", arg_rust_get_fps()); }
static const struct kernel_param_ops fps_ops = { .set=arg_set_fps, .get=arg_get_fps };
module_param_cb(fps, &fps_ops, NULL, 0644);
MODULE_PARM_DESC(fps, "1=FPS-stability mode  0=off");

/* ── Module init ─────────────────────────────────────────────── */
static int __init arg_init(void)
{
	int ret;

	pr_info("ARG Scheduler Layer v1.0 loading\n");
	pr_info("Rust+ASM module | Linux 4.19 | ARM64 | Exynos 850\n");

	/* 1. Initialise Rust subsystems */
	ret = arg_rust_init();
	if (ret) {
		pr_err("Rust init failed: %d\n", ret);
		return ret;
	}

	/* 2. CPU hotplug notifier */
	register_cpu_notifier(&arg_cpu_nb);

	/* 3. Arm hooks — smp_store_release pairs with smp_load_acquire
	 *    in sched.h inline helpers.  Order matters: set the function
	 *    pointers BEFORE setting arg_enabled = true.               */
	smp_store_release(&arg_pick_next_hook,   arg_do_pick_next);
	smp_store_release(&arg_enqueue_hook,     arg_do_enqueue);
	smp_store_release(&arg_wakeup_hook,      arg_do_wakeup);
	smp_store_release(&arg_update_load_hook, arg_do_update_load);
	smp_store_release(&arg_enabled, true);

	pr_info("loaded. async=%d pack=%d energy=%d fps=%d\n",
		arg_rust_get_async(), arg_rust_get_pack(),
		arg_rust_get_energy(), arg_rust_get_fps());
	return 0;
}

/* ── Module exit ─────────────────────────────────────────────── */
static void __exit arg_exit(void)
{
	pr_info("unloading…\n");

	/* Disarm: clear enable first, then pointers */
	smp_store_release(&arg_enabled, false);
	smp_store_release(&arg_pick_next_hook,   NULL);
	smp_store_release(&arg_enqueue_hook,     NULL);
	smp_store_release(&arg_wakeup_hook,      NULL);
	smp_store_release(&arg_update_load_hook, NULL);

	/* Wait for any CPU still inside a hook to exit */
	synchronize_sched();

	unregister_cpu_notifier(&arg_cpu_nb);

	/* Tear down Rust subsystems */
	arg_rust_exit();

	pr_info("unloaded cleanly\n");
}

module_init(arg_init);
module_exit(arg_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ARG Project");
MODULE_DESCRIPTION("ARG Scheduler Layer — Rust+ASM runtime optimizer");
MODULE_VERSION("1.0");
