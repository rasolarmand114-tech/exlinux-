// SPDX-License-Identifier: GPL-2.0
/*
 * arg/hook/shim.c — Kernel Accessor Shims
 *
 * Every crossing from Rust into the kernel goes through these thin
 * wrappers.  They translate between Rust's opaque void* handles and
 * actual kernel types, and provide the kernel data structures (hash
 * table, pack bins) that Rust cannot allocate with native kernel APIs.
 *
 * Rust NEVER dereferences kernel pointers.  This file is the only
 * place in the module that touches kernel struct internals.
 */

#define pr_fmt(fmt) "arg/shim: " fmt

#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include "../include/arg_abi.h"

/* GFP_ATOMIC value — keep in sync with bindings.rs */
#define ARG_GFP_KERNEL 0x0014000cu
#define ARG_GFP_ATOMIC 0x00000400u

/* ── Task accessors ─────────────────────────────────────────────── */

u32 arg_shim_task_util(void *p)
{
	return (u32)READ_ONCE(((struct task_struct *)p)->se.avg.util_avg);
}

int arg_shim_task_cpu(void *p)
{
	return task_cpu((struct task_struct *)p);
}

u32 arg_shim_task_policy(void *p)
{
	return (u32)((struct task_struct *)p)->policy;
}

int arg_shim_task_pid(void *p)
{
	return (int)task_pid_nr((struct task_struct *)p);
}

bool arg_shim_task_is_rt(void *p)
{
	unsigned int pol = ((struct task_struct *)p)->policy;
	return pol == SCHED_FIFO || pol == SCHED_RR;
}

bool arg_shim_task_is_dl(void *p)
{
	return ((struct task_struct *)p)->policy == SCHED_DEADLINE;
}

int arg_shim_task_cpu_allowed(void *p, int cpu)
{
	return cpumask_test_cpu(cpu, &((struct task_struct *)p)->cpus_allowed);
}

/* ── Runqueue accessors ─────────────────────────────────────────── */

void *arg_shim_cpu_rq(int cpu)
{
	return (void *)cpu_rq(cpu);
}

int arg_shim_rq_cpu(void *rq)
{
	return cpu_of((struct rq *)rq);
}

u32 arg_shim_rq_nr_running(void *rq)
{
	return (u32)((struct rq *)rq)->nr_running;
}

u32 arg_shim_rq_rt_nr(void *rq)
{
	return (u32)((struct rq *)rq)->rt.rt_nr_running;
}

/* ── CPU topology / utilization ─────────────────────────────────── */

u32 arg_shim_cpu_util(int cpu)
{
	unsigned long util = READ_ONCE(cpu_rq(cpu)->cfs.avg.util_avg);
	unsigned long cap  = arch_scale_cpu_capacity(NULL, cpu);
	return (u32)(util > cap ? cap : util);
}

u32 arg_shim_cpu_capacity(int cpu)
{
	return (u32)arch_scale_cpu_capacity(NULL, cpu);
}

bool arg_shim_cpu_idle(int cpu)
{
	return idle_cpu(cpu);
}

bool arg_shim_cpu_online(int cpu)
{
	return cpu_online(cpu);
}

int arg_shim_nr_cpus(void)
{
	return nr_cpu_ids;
}

int arg_shim_this_cpu(void)
{
	return smp_processor_id();
}

/* ── Memory ─────────────────────────────────────────────────────── */

void *arg_shim_kmalloc(size_t size, u32 flags)
{
	return kmalloc(size, (gfp_t)flags);
}

void arg_shim_kfree(void *ptr)
{
	kfree(ptr);
}

u64 arg_shim_ktime_ns(void)
{
	return ktime_get_ns();
}

void arg_shim_sync_sched(void)
{
	synchronize_sched();
}

/* ── Task-state hash table ──────────────────────────────────────── */
/*
 * Stores lightweight per-task metadata that the Rust policy layer
 * reads and writes.  The struct lives in kernel slab memory.
 */

struct arg_ts {
	void             *task;        /* key: struct task_struct * */
	u32               util;
	u32               migrations;
	u8                is_large;
	u64               enqueue_ns;
	u64               wakeup_ns;
	struct hlist_node node;
};

#define ARG_HT_BITS 8          /* 256 buckets */
static DECLARE_HASHTABLE(arg_ht, ARG_HT_BITS);
static DEFINE_SPINLOCK(arg_ht_lock);
static struct kmem_cache *arg_ts_cache __read_mostly;

int arg_shim_ts_init(void)
{
	arg_ts_cache = kmem_cache_create("arg_ts", sizeof(struct arg_ts),
					 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!arg_ts_cache)
		return -ENOMEM;
	hash_init(arg_ht);
	return 0;
}

void arg_shim_ts_destroy(void)
{
	struct arg_ts *ts;
	struct hlist_node *tmp;
	unsigned long flags;
	int bkt;

	if (!arg_ts_cache)
		return;

	spin_lock_irqsave(&arg_ht_lock, flags);
	hash_for_each_safe(arg_ht, bkt, tmp, ts, node) {
		hash_del(&ts->node);
		kmem_cache_free(arg_ts_cache, ts);
	}
	spin_unlock_irqrestore(&arg_ht_lock, flags);

	kmem_cache_destroy(arg_ts_cache);
	arg_ts_cache = NULL;
}

void *arg_shim_ts_get(void *task)
{
	struct arg_ts *ts;
	u32 key = (u32)(unsigned long)task;
	unsigned long flags;

	spin_lock_irqsave(&arg_ht_lock, flags);
	hash_for_each_possible(arg_ht, ts, node, key) {
		if (ts->task == task) {
			spin_unlock_irqrestore(&arg_ht_lock, flags);
			return ts;
		}
	}
	spin_unlock_irqrestore(&arg_ht_lock, flags);
	return NULL;
}

void *arg_shim_ts_create(void *task)
{
	struct arg_ts *ts;
	u32 key = (u32)(unsigned long)task;
	unsigned long flags;

	ts = kmem_cache_zalloc(arg_ts_cache, GFP_ATOMIC);
	if (!ts)
		return NULL;

	ts->task = task;
	spin_lock_irqsave(&arg_ht_lock, flags);
	hash_add(arg_ht, &ts->node, key);
	spin_unlock_irqrestore(&arg_ht_lock, flags);
	return ts;
}

void arg_shim_ts_remove(void *task)
{
	struct arg_ts *ts;
	u32 key = (u32)(unsigned long)task;
	unsigned long flags;

	spin_lock_irqsave(&arg_ht_lock, flags);
	hash_for_each_possible(arg_ht, ts, node, key) {
		if (ts->task == task) {
			hash_del(&ts->node);
			spin_unlock_irqrestore(&arg_ht_lock, flags);
			kmem_cache_free(arg_ts_cache, ts);
			return;
		}
	}
	spin_unlock_irqrestore(&arg_ht_lock, flags);
}

/* Field accessors — Rust uses opaque void* and calls these */
u32  arg_shim_ts_get_util(void *ts)        { return ((struct arg_ts *)ts)->util; }
void arg_shim_ts_set_util(void *ts, u32 v) { ((struct arg_ts *)ts)->util = v; }
u8   arg_shim_ts_get_large(void *ts)       { return ((struct arg_ts *)ts)->is_large; }
void arg_shim_ts_set_large(void *ts, u8 v) { ((struct arg_ts *)ts)->is_large = v; }
u32  arg_shim_ts_get_mig(void *ts)         { return ((struct arg_ts *)ts)->migrations; }
void arg_shim_ts_inc_mig(void *ts)         { ((struct arg_ts *)ts)->migrations++; }
u64  arg_shim_ts_get_enq(void *ts)         { return ((struct arg_ts *)ts)->enqueue_ns; }
void arg_shim_ts_set_enq(void *ts, u64 v)  { ((struct arg_ts *)ts)->enqueue_ns = v; }
u64  arg_shim_ts_get_wake(void *ts)        { return ((struct arg_ts *)ts)->wakeup_ns; }
void arg_shim_ts_set_wake(void *ts, u64 v) { ((struct arg_ts *)ts)->wakeup_ns = v; }

/* ── Pack bins (per-CPU aggregate utilization) ──────────────────── */
/* Simple 8-element array; accessed by CPU index.
 * Protected by the caller holding rq->lock or being single-threaded
 * during init/exit.                                                   */
#define ARG_MAX_CPU 8
static u32 pack_util[ARG_MAX_CPU];

u32  arg_shim_pack_get(int cpu)        { if (cpu < ARG_MAX_CPU) return READ_ONCE(pack_util[cpu]); return 0; }
void arg_shim_pack_add(int cpu, u32 u) { if (cpu < ARG_MAX_CPU) WRITE_ONCE(pack_util[cpu], pack_util[cpu] + u); }
void arg_shim_pack_sub(int cpu, u32 u) { if (cpu < ARG_MAX_CPU && pack_util[cpu] >= u) WRITE_ONCE(pack_util[cpu], pack_util[cpu] - u); }
void arg_shim_pack_reset(int cpu)      { if (cpu < ARG_MAX_CPU) WRITE_ONCE(pack_util[cpu], 0); }

/* ── kthread helpers ────────────────────────────────────────────── */

void *arg_shim_kthread_create_cpu(int cpu, const char *name)
{
	struct task_struct *t;

	if (!cpu_online(cpu))
		return NULL;

	t = kthread_create(NULL, NULL, "%s/%d", name, cpu);
	if (IS_ERR(t))
		return NULL;

	kthread_bind(t, cpu);
	return (void *)t;
}

void arg_shim_kthread_stop(void *task)
{
	if (task)
		kthread_stop((struct task_struct *)task);
}

void arg_shim_kthread_wake(void *task)
{
	if (task)
		wake_up_process((struct task_struct *)task);
}

/* ── SMP ────────────────────────────────────────────────────────── */

void arg_shim_smp_call(int cpu, void (*fn)(void *), void *arg, int wait)
{
	smp_call_function_single(cpu, fn, arg, wait);
}

/* ── Logging ─────────────────────────────────────────────────────
 * ptr: valid UTF-8, NOT null-terminated, len bytes             */

void arg_shim_log_info(const u8 *ptr, size_t len)
{
	printk(KERN_INFO "arg_sched: %.*s\n", (int)len, ptr);
}

void arg_shim_log_debug(const u8 *ptr, size_t len)
{
	pr_debug("%.*s\n", (int)len, ptr);
}
