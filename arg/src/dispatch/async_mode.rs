// SPDX-License-Identifier: GPL-2.0
//! Async (Gaming / Low-Latency) CPU Selection
//! arg_async=1 → this policy is active.
//! Strategy: lowest-utilization CPU → minimal queueing latency.
use crate::{bindings, state::cpu_state};
use crate::topology::smp;

pub fn select_cpu(p: *mut bindings::Task) -> Option<usize> {
    let p_util = unsafe { bindings::arg_shim_task_util(p) };
    let mut best     = None;
    let mut min_util = u32::MAX;

    smp::for_each_online(|cpu| {
        let rq = unsafe { bindings::arg_shim_cpu_rq(cpu as i32) };
        if !rq.is_null() && unsafe { bindings::arg_shim_rq_rt_nr(rq) } > 0 { return; }
        if unsafe { bindings::arg_shim_task_cpu_allowed(p, cpu as i32) } == 0 { return; }

        // big.LITTLE: steer big tasks to big cores
        let is_big = cpu_state::is_big(cpu);
        if p_util > bindings::LARGE_TASK_UTIL && !is_big { return; }
        if p_util < bindings::PACK_TASK_UTIL_MAX && is_big { return; }

        let u = cpu_state::util(cpu);
        if u < min_util {
            min_util = u;
            best = Some(cpu);
        }
    });
    best
}
