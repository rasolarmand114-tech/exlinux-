// SPDX-License-Identifier: GPL-2.0
//! Sync (Throughput / Batch) CPU Selection
//! arg_async=0 → this policy is active.
//! Strategy: stay on prev CPU (cache warm) or tightest-fitting bin.
use crate::{bindings, state::cpu_state};
use crate::topology::smp;

pub fn select_cpu(p: *mut bindings::Task) -> Option<usize> {
    let p_util   = unsafe { bindings::arg_shim_task_util(p) };
    let prev_cpu = unsafe { bindings::arg_shim_task_cpu(p) } as usize;

    // 1. Stay on previous CPU if it has headroom (cache warmth)
    if smp::is_online(prev_cpu)
        && unsafe { bindings::arg_shim_task_cpu_allowed(p, prev_cpu as i32) } != 0
        && cpu_state::util(prev_cpu) + p_util <= bindings::PACK_BIN_UTIL_MAX
    {
        return Some(prev_cpu);
    }

    // 2. Tightest-fitting CPU (maximise fill, minimise idle CPUs)
    let mut best      = None;
    let mut min_room  = u32::MAX;

    smp::for_each_online(|cpu| {
        let rq = unsafe { bindings::arg_shim_cpu_rq(cpu as i32) };
        if !rq.is_null() && unsafe { bindings::arg_shim_rq_rt_nr(rq) } > 0 { return; }
        if unsafe { bindings::arg_shim_task_cpu_allowed(p, cpu as i32) } == 0 { return; }

        let room = cpu_state::headroom(cpu);
        if room >= p_util && room < min_room {
            min_room = room;
            best = Some(cpu);
        }
    });
    best
}
