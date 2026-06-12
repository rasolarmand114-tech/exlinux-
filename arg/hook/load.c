// SPDX-License-Identifier: GPL-2.0
/*
 * arg/hook/load.c — load-average hook + CPU hotplug glue (C → Rust)
 *
 * arg_do_update_load() is called from kernel/sched/loadavg.c
 * at the LOAD_FREQ tick.
 *
 * CPU hotplug callbacks are registered by glue.c (arg_core.c)
 * and forwarded to Rust here.
 */
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include "../include/arg_abi.h"

void arg_do_update_load(void)
{
	arg_rust_update_load();
}

/* CPU hotplug notifier — registered in arg_core.c */
int arg_cpu_notify(struct notifier_block *nb,
		   unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		arg_rust_cpu_online((int)cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		arg_rust_cpu_offline((int)cpu);
		break;
	}
	return NOTIFY_OK;
}
