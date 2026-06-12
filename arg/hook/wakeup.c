// SPDX-License-Identifier: GPL-2.0
/*
 * arg/hook/wakeup.c — wakeup hook (C bridge → Rust)
 *
 * Called from kernel/sched/core.c:try_to_wake_up() on
 * successful wakeup of a non-RT task.
 */
#include <linux/sched.h>
#include "../include/arg_abi.h"

void arg_do_wakeup(struct rq *rq, struct task_struct *p)
{
	arg_rust_wakeup((void *)rq, (void *)p);
}
