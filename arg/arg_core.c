// SPDX-License-Identifier: GPL-2.0
/*
 * ARG Scheduler Layer — Kernel Module Core
 * Version: 1.0
 * Target:  Linux 4.19 · ARM64 · Exynos 850
 *
 * This module registers itself with the ARG hook infrastructure
 * (CONFIG_SCHAD_ARG) that lives in kernel/sched/core.c.
 * On load  → sets arg_pick_next_hook, arg_enqueue_hook, arg_wakeup_hook
 * On unload → clears them, waits for any in-flight call to finish
 */

#define pr_fmt(fmt) "arg_sched: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>

/* Pull in the hook symbols from kernel/sched/arg_hook.c */
#include <linux/sched.h>

#include "arg_internal.h"

/* ------------------------------------------------------------------ */
/* Symbols exported from kernel/sched/arg_hook.c (via kernel patch)  */
/* ------------------------------------------------------------------ */
extern bool arg_enabled;
extern struct task_struct *(*arg_pick_next_hook)(struct rq *rq);
extern void (*arg_enqueue_hook)(struct rq *rq, struct task_struct *p);
extern void (*arg_wakeup_hook)(struct rq *rq, struct task_struct *p);
extern void (*arg_update_load_hook)(void);

/* ------------------------------------------------------------------ */
/* Global state                                                        */
/* ------------------------------------------------------------------ */
DEFINE_PER_CPU(struct arg_cpu_features, arg_cpu_feat);
DEFINE_PER_CPU(struct arg_cpu_state, arg_cpu_st);

DEFINE_HASHTABLE(arg_task_ht, ARG_TASK_HT_BITS);
DEFINE_SPINLOCK(arg_task_ht_lock);

struct arg_pack_bin arg_pack_bins[NR_CPUS];

/* Runtime flags — module_param exposed to sysfs */
atomic_t arg_async_mode      = ATOMIC_INIT(1);  /* default: async */
atomic_t arg_pack_enabled    = ATOMIC_INIT(1);  /* default: on    */
atomic_t arg_vthread_enabled = ATOMIC_INIT(1);  /* default: on    */
atomic_t arg_energy_mode     = ATOMIC_INIT(0);  /* default: off   */
atomic_t arg_fps_mode        = ATOMIC_INIT(0);  /* default: off   */

/* ------------------------------------------------------------------ */
/* CPU hotplug notifier                                                */
/* ------------------------------------------------------------------ */
static int arg_cpu_callback(struct notifier_block *nfb,
			    unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		arg_topology_cpu_up(cpu);
		arg_detect_cpu_features(cpu);
		pr_debug("CPU%u online\n", cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		arg_topology_cpu_down(cpu);
		pr_debug("CPU%u offline\n", cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block arg_cpu_nb = {
	.notifier_call = arg_cpu_callback,
	.priority      = INT_MIN,  /* be last; do not interfere with cpufreq */
};

/* ------------------------------------------------------------------ */
/* module_param wrappers                                               */
/* ------------------------------------------------------------------ */
static int arg_set_async(const char *val, const struct kernel_param *kp)
{
	int v;
	int ret = kstrtoint(val, 10, &v);

	if (ret)
		return ret;
	if (v != 0 && v != 1)
		return -EINVAL;

	atomic_set(&arg_async_mode, v);
	pr_info("async_mode → %d\n", v);
	return 0;
}
static int arg_get_async(char *buf, const struct kernel_param *kp)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 atomic_read(&arg_async_mode));
}
static const struct kernel_param_ops arg_async_ops = {
	.set = arg_set_async,
	.get = arg_get_async,
};
module_param_cb(async, &arg_async_ops, NULL, 0644);
MODULE_PARM_DESC(async, "1=async(gaming) 0=sync(throughput)");

static int arg_set_pack(const char *val, const struct kernel_param *kp)
{
	int v;
	int ret = kstrtoint(val, 10, &v);

	if (ret)
		return ret;
	atomic_set(&arg_pack_enabled, !!v);
	pr_info("pack_enabled → %d\n", !!v);
	return 0;
}
static int arg_get_pack(char *buf, const struct kernel_param *kp)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 atomic_read(&arg_pack_enabled));
}
static const struct kernel_param_ops arg_pack_ops = {
	.set = arg_set_pack, .get = arg_get_pack,
};
module_param_cb(pack, &arg_pack_ops, NULL, 0644);
MODULE_PARM_DESC(pack, "1=enable task packing (saves energy)");

/* ------------------------------------------------------------------ */
/* Module init / exit                                                  */
/* ------------------------------------------------------------------ */
static int __init arg_init(void)
{
	int ret, cpu;

	pr_info("ARG Scheduler Layer v" ARG_VERSION_STR " loading\n");
	pr_info("Target: Linux 4.19 · ARM64 · Exynos 850\n");

	/* 1. Per-CPU state */
	for_each_possible_cpu(cpu) {
		struct arg_cpu_state *cs = &per_cpu(arg_cpu_st, cpu);

		spin_lock_init(&cs->lock);
		cs->capacity = arch_scale_cpu_capacity(NULL, cpu);
		cs->util     = 0;
		cs->is_idle  = idle_cpu(cpu);
	}

	/* 2. Detect ARM64 features per-CPU */
	for_each_online_cpu(cpu)
		arg_detect_cpu_features(cpu);

	/* 3. Initialise topology */
	ret = arg_topology_init();
	if (ret) {
		pr_err("topology_init failed: %d\n", ret);
		return ret;
	}

	/* 4. Task state subsystem */
	ret = arg_state_init();
	if (ret) {
		pr_err("state_init failed: %d\n", ret);
		goto err_topo;
	}

	/* 5. Pack bins */
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		spin_lock_init(&arg_pack_bins[cpu].lock);
		arg_pack_bins[cpu].cpu        = cpu;
		arg_pack_bins[cpu].total_util = 0;
		arg_pack_bins[cpu].nr_tasks   = 0;
	}

	/* 6. Virtual Thread layer */
	ret = arg_vthread_init();
	if (ret) {
		pr_err("vthread_init failed: %d\n", ret);
		goto err_state;
	}

	/* 7. Runtime sysfs parameters */
	arg_runtime_init();

	/* 8. Register hotplug notifier */
	register_cpu_notifier(&arg_cpu_nb);

	/* 9. Arm the hooks — must be last, after all subsystems are ready.
	 *    Use smp_store_release so every CPU sees a fully-constructed
	 *    subsystem before arg_enabled is visible as true.
	 */
	smp_store_release(&arg_pick_next_hook, arg_do_pick_next);
	smp_store_release(&arg_enqueue_hook,   arg_do_enqueue);
	smp_store_release(&arg_wakeup_hook,    arg_do_wakeup);
	smp_store_release(&arg_update_load_hook, arg_do_update_load);
	smp_store_release(&arg_enabled, true);

	pr_info("loaded. async=%d pack=%d vthread=%d\n",
		atomic_read(&arg_async_mode),
		atomic_read(&arg_pack_enabled),
		atomic_read(&arg_vthread_enabled));
	return 0;

err_state:
	arg_state_exit();
err_topo:
	arg_topology_exit();
	return ret;
}

static void __exit arg_exit(void)
{
	pr_info("unloading…\n");

	/* Disarm hooks first.  Pairs with the smp_store_release above. */
	smp_store_release(&arg_enabled, false);
	smp_store_release(&arg_pick_next_hook, NULL);
	smp_store_release(&arg_enqueue_hook,   NULL);
	smp_store_release(&arg_wakeup_hook,    NULL);
	smp_store_release(&arg_update_load_hook, NULL);

	/*
	 * Wait for all CPUs to finish any in-flight hook call.
	 * synchronize_sched() guarantees no CPU is inside an RCU
	 * read-side critical section — safe for scheduler context.
	 */
	synchronize_sched();

	unregister_cpu_notifier(&arg_cpu_nb);
	arg_vthread_exit();
	arg_state_exit();
	arg_topology_exit();
	arg_runtime_exit();

	pr_info("unloaded cleanly\n");
}

module_init(arg_init);
module_exit(arg_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ARG Project");
MODULE_DESCRIPTION("ARG Scheduler Layer — Runtime optimization for Linux 4.19/ARM64");
MODULE_VERSION(ARG_VERSION_STR);
