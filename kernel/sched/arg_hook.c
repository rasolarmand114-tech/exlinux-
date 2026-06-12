// SPDX-License-Identifier: GPL-2.0-only
/*
 * ARG Scheduler Hook – Variable Definitions
 *
 * Exports function pointers that the ARG Rust module (arg/)
 * registers at init. All are __read_mostly as they change
 * only during module load/unload.
 */
#include <linux/export.h>
#include <linux/compiler.h>
#include "sched.h"

/* Master enable – checked first in every fast path */
bool arg_enabled __read_mostly = false;
EXPORT_SYMBOL_GPL(arg_enabled);

/* pick_next_task override (must return fair task or NULL) */
struct task_struct *(*arg_pick_next_hook)(struct rq *rq)
	__read_mostly = NULL;
EXPORT_SYMBOL_GPL(arg_pick_next_hook);

/* enqueue notification */
void (*arg_enqueue_hook)(struct rq *rq, struct task_struct *p)
	__read_mostly = NULL;
EXPORT_SYMBOL_GPL(arg_enqueue_hook);

/* wakeup hint (CPU selected, under rq->lock) */
void (*arg_wakeup_hook)(struct rq *rq, struct task_struct *p)
	__read_mostly = NULL;
EXPORT_SYMBOL_GPL(arg_wakeup_hook);

/* Periodic load-update tick */
void (*arg_update_load_hook)(void) __read_mostly = NULL;
EXPORT_SYMBOL_GPL(arg_update_load_hook);

/* RT task enqueue (debug / verification only) */
void (*arg_rt_enqueue_hook)(struct rq *rq, struct task_struct *p)
	__read_mostly = NULL;
EXPORT_SYMBOL_GPL(arg_rt_enqueue_hook);
