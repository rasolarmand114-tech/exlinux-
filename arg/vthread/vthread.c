// SPDX-License-Identifier: GPL-2.0
/*
 * arg/vthread/vthread.c — ARG Virtual Thread Layer
 *
 * A Virtual Thread (vthread) represents a logical worker that handles
 * a slice of a large task's load on a dedicated CPU.
 *
 * Rules:
 *   - One Thread → One CPU At A Time (ALWAYS preserved)
 *   - A vthread is NOT the original task running on multiple CPUs
 *   - A vthread is a separate kthread worker assigned to a specific CPU
 *   - The parent task coordinates via shared memory (no shared context)
 *
 * Use case: task with util > ARG_LARGE_TASK_UTIL that needs to spread
 *           work across idle CPUs (e.g. game asset loading, decompression)
 *
 * NOTE: Rust port target → vthread/worker.rs, vthread/pool.rs
 */

#define pr_fmt(fmt) "arg/vthread: " fmt

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include "../arg_internal.h"

static LIST_HEAD(arg_vthread_pool);
static DEFINE_SPINLOCK(arg_vt_pool_lock);
static struct kmem_cache *arg_vt_cache __read_mostly;
static atomic_t arg_vt_id_counter = ATOMIC_INIT(0);

int arg_vthread_init(void)
{
	if (!arg_runtime_is_vthread_enabled())
		return 0;

	arg_vt_cache = kmem_cache_create(
		"arg_vthread",
		sizeof(struct arg_vthread),
		0,
		SLAB_HWCACHE_ALIGN,
		NULL);

	if (!arg_vt_cache)
		return -ENOMEM;

	pr_info("virtual thread layer ready\n");
	return 0;
}

void arg_vthread_exit(void)
{
	struct arg_vthread *vt, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&arg_vt_pool_lock, flags);
	list_for_each_entry_safe(vt, tmp, &arg_vthread_pool, node) {
		list_del(&vt->node);
		if (vt->worker && !IS_ERR(vt->worker))
			kthread_stop(vt->worker);
		kmem_cache_free(arg_vt_cache, vt);
	}
	spin_unlock_irqrestore(&arg_vt_pool_lock, flags);

	if (arg_vt_cache) {
		kmem_cache_destroy(arg_vt_cache);
		arg_vt_cache = NULL;
	}
	pr_info("virtual thread layer stopped\n");
}

struct arg_vthread *arg_vthread_create(struct task_struct *parent, int cpu)
{
	struct arg_vthread *vt;
	unsigned long flags;

	if (!arg_vt_cache || !cpu_online(cpu))
		return NULL;

	vt = kmem_cache_zalloc(arg_vt_cache, GFP_KERNEL);
	if (!vt)
		return NULL;

	vt->id         = atomic_inc_return(&arg_vt_id_counter);
	vt->target_cpu = cpu;
	vt->load       = arg_task_util(parent) / 2;
	atomic_set(&vt->state, ARG_VT_IDLE);
	INIT_LIST_HEAD(&vt->node);

	/*
	 * The worker kthread is pinned to target_cpu.
	 * It processes work items queued by the parent task.
	 * One Thread → One CPU At A Time: worker runs only on target_cpu.
	 */
	vt->worker = kthread_create(NULL, NULL, "arg_vt/%d", vt->id);
	if (IS_ERR(vt->worker)) {
		kmem_cache_free(arg_vt_cache, vt);
		return NULL;
	}
	kthread_bind(vt->worker, cpu);

	spin_lock_irqsave(&arg_vt_pool_lock, flags);
	list_add_tail(&vt->node, &arg_vthread_pool);
	spin_unlock_irqrestore(&arg_vt_pool_lock, flags);

	pr_debug("vthread %d created for CPU%d\n", vt->id, cpu);
	return vt;
}

void arg_vthread_destroy(struct arg_vthread *vt)
{
	unsigned long flags;

	if (!vt)
		return;

	atomic_set(&vt->state, ARG_VT_DEAD);

	spin_lock_irqsave(&arg_vt_pool_lock, flags);
	list_del(&vt->node);
	spin_unlock_irqrestore(&arg_vt_pool_lock, flags);

	if (vt->worker && !IS_ERR(vt->worker))
		kthread_stop(vt->worker);

	pr_debug("vthread %d destroyed\n", vt->id);
	kmem_cache_free(arg_vt_cache, vt);
}
