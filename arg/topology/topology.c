// SPDX-License-Identifier: GPL-2.0
/*
 * arg/topology/topology.c — CPU Topology Layer
 *
 * Supports:
 *   SMP · MC · big.LITTLE · CPU Hotplug · Heterogeneous CPUs
 *
 * On Exynos 850 (Cortex-A55 × 8, homogeneous SMP):
 *   - All cores have the same capacity (1024)
 *   - is_big = false for all
 *
 * On big.LITTLE devices (future):
 *   - Big cores (Cortex-A7x) have higher capacity values
 *   - LITTLE cores have lower capacity
 *   - Detected via arch_scale_cpu_capacity()
 *
 * NOTE: Rust port target → topology/smp.rs, topology/cluster.rs
 */

#define pr_fmt(fmt) "arg/topology: " fmt

#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include "../arg_internal.h"

/* Exynos 850: 8× Cortex-A55 @ up to 2.0 GHz
 * Capacity reported by arch_scale_cpu_capacity is 1024 for all cores.
 * We store the detected capacity at init time.                        */

/* Threshold above which a CPU is considered "big" in big.LITTLE */
#define ARG_BIG_CORE_CAPACITY_THRESHOLD   800U   /* out of 1024 */

int arg_topology_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct arg_cpu_state *cs = &per_cpu(arg_cpu_st, cpu);
		unsigned int cap = arch_scale_cpu_capacity(NULL, cpu);

		spin_lock_init(&cs->lock);
		cs->capacity       = cap;
		cs->is_big         = (cap >= ARG_BIG_CORE_CAPACITY_THRESHOLD);
		cs->hotplugged_out = !cpu_online(cpu);
		cs->util           = 0;
		cs->is_idle        = true;
		cs->last_idle_ts   = 0;
	}

	pr_info("topology initialised for %d CPUs\n",
		num_possible_cpus());

	for_each_online_cpu(cpu) {
		struct arg_cpu_state *cs = &per_cpu(arg_cpu_st, cpu);

		pr_info("CPU%d: capacity=%u big=%d\n",
			cpu, cs->capacity, cs->is_big);
	}
	return 0;
}

void arg_topology_exit(void)
{
	/* Nothing to free — all data is per-CPU static */
}

void arg_topology_cpu_up(int cpu)
{
	struct arg_cpu_state *cs;
	unsigned long flags;

	if (cpu >= nr_cpu_ids)
		return;

	cs = &per_cpu(arg_cpu_st, cpu);
	spin_lock_irqsave(&cs->lock, flags);
	cs->capacity       = arch_scale_cpu_capacity(NULL, cpu);
	cs->is_big         = (cs->capacity >= ARG_BIG_CORE_CAPACITY_THRESHOLD);
	cs->hotplugged_out = false;
	cs->util           = 0;
	cs->is_idle        = true;
	spin_unlock_irqrestore(&cs->lock, flags);

	pr_debug("CPU%d online capacity=%u\n", cpu, cs->capacity);
}

void arg_topology_cpu_down(int cpu)
{
	struct arg_cpu_state *cs;
	unsigned long flags;

	if (cpu >= nr_cpu_ids)
		return;

	cs = &per_cpu(arg_cpu_st, cpu);
	spin_lock_irqsave(&cs->lock, flags);
	cs->hotplugged_out = true;
	cs->util           = 0;
	cs->is_idle        = true;
	spin_unlock_irqrestore(&cs->lock, flags);

	/* Clean up pack bin for this CPU */
	spin_lock(&arg_pack_bins[cpu].lock);
	arg_pack_bins[cpu].total_util = 0;
	arg_pack_bins[cpu].nr_tasks   = 0;
	spin_unlock(&arg_pack_bins[cpu].lock);

	pr_debug("CPU%d offline\n", cpu);
}

unsigned int arg_topology_cpu_capacity(int cpu)
{
	if (cpu < 0 || cpu >= nr_cpu_ids)
		return 0;
	return per_cpu(arg_cpu_st, cpu).capacity;
}

bool arg_topology_is_big_cpu(int cpu)
{
	if (cpu < 0 || cpu >= nr_cpu_ids)
		return false;
	return per_cpu(arg_cpu_st, cpu).is_big;
}
