// SPDX-License-Identifier: GPL-2.0
//! FPS-Stability Policy
//! Keep render/game thread on the highest-capacity available core.
//! Used in gaming mode (arg_async=1, arg_fps=1).
use crate::{bindings, state::cpu_state};
use crate::topology::smp;

pub fn select_cpu(p: *mut bindings::Task) -> Option<usize> {
    let p_util = unsafe { bindings::arg_shim_task_util(p) };
    let mut best    = None;
    let mut max_cap = 0u32;

    smp::for_each_online(|cpu| {
        let rq = unsafe { bindings::arg_shim_cpu_rq(cpu as i32) };
        if !rq.is_null() && unsafe { bindings::arg_shim_rq_rt_nr(rq) } > 0 { return; }
        if unsafe { bindings::arg_shim_task_cpu_allowed(p, cpu as i32) } == 0 { return; }
        if cpu_state::headroom(cpu) < p_util { return; }

        let cap = cpu_state::cap(cpu);
        if cap > max_cap {
            max_cap = cap;
            best    = Some(cpu);
        }
    });
    best
}
