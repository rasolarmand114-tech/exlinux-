// SPDX-License-Identifier: GPL-2.0
//! Energy-Aware CPU Selection
//! Prefer smallest-capacity CPU that can fit the task.
//! Large tasks are rejected (caller falls back to async/sync).
use crate::{bindings, state::cpu_state};
use crate::topology::smp;

pub fn select_cpu(p: *mut bindings::Task) -> Option<usize> {
    let p_util = unsafe { bindings::arg_shim_task_util(p) };
    if p_util > bindings::LARGE_TASK_UTIL { return None; }  // too big

    let mut best    = None;
    let mut min_cap = u32::MAX;

    smp::for_each_online(|cpu| {
        let rq = unsafe { bindings::arg_shim_cpu_rq(cpu as i32) };
        if !rq.is_null() && unsafe { bindings::arg_shim_rq_rt_nr(rq) } > 0 { return; }
        if unsafe { bindings::arg_shim_task_cpu_allowed(p, cpu as i32) } == 0 { return; }
        if cpu_state::headroom(cpu) < p_util { return; }

        let cap = cpu_state::cap(cpu);
        if cap < min_cap {
            min_cap = cap;
            best    = Some(cpu);
        }
    });
    best
}
