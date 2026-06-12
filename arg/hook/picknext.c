// SPDX-License-Identifier: GPL-2.0
/*
 * arg/hook/picknext.c — pick_next hook (C bridge → Rust)
 *
 * Called from kernel/sched/core.c:pick_next_task() when
 * CONFIG_SCHAD_ARG is enabled and rt_nr_running == 0.
 *
 * Returns:
 *   non-NULL → use this task (Rust made a decision)
 *   NULL     → let CFS decide normally
 */
#include <linux/sched.h>
#include "../include/arg_abi.h"

struct task_struct *arg_do_pick_next(struct rq *rq)
{
	return (struct task_struct *)arg_rust_pick_next((void *)rq);
}
