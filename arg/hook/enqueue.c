// SPDX-License-Identifier: GPL-2.0
/*
 * arg/hook/enqueue.c — enqueue hook (C bridge → Rust)
 *
 * Called from kernel/sched/core.c:enqueue_task() before
 * the task is handed to its sched_class.
 */
#include <linux/sched.h>
#include "../include/arg_abi.h"

void arg_do_enqueue(struct rq *rq, struct task_struct *p)
{
	arg_rust_enqueue((void *)rq, (void *)p);
}
