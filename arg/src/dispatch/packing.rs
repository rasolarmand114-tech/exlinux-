// SPDX-License-Identifier: GPL-2.0
//! Task Packing Policy
//! Goal: consolidate small tasks onto as few CPUs as possible.
//!
//!  Task A = 5%  ┐
//!  Task B = 8%  ├─ packed onto ONE core → 2 cores go idle → C-states
//!  Task C = 10% ┘
//!
//! "One Thread → One CPU At A Time" invariant is preserved:
//! we choose a CPU for the task, never split a thread.
use crate::{bindings, state::cpu_state};
use crate::topology::smp;

/// Find the best CPU to pack task p (util ≤ PACK_TASK_UTIL_MAX).
/// Strategy: most-loaded bin that still has headroom (maximise consolidation).
/// Returns None if no suitable CPU found.
pub fn find_cpu(task_util: u32) -> Option<usize> {
    if task_util > bindings::PACK_TASK_UTIL_MAX {
        return None;  // not a small task
    }
    let mut best_cpu  = None;
    let mut best_util = 0u32;

    smp::for_each_online(|cpu| {
        // Skip CPUs with RT tasks
        let rq = unsafe { bindings::arg_shim_cpu_rq(cpu as i32) };
        if !rq.is_null() && unsafe { bindings::arg_shim_rq_rt_nr(rq) } > 0 {
            return;
        }
        let bin_util = cpu_state::util(cpu);
        // Must fit without overloading
        if bin_util + task_util > bindings::PACK_BIN_UTIL_MAX {
            return;
        }
        // Pick most-loaded bin that still fits (consolidation)
        if bin_util >= best_util {
            best_util = bin_util;
            best_cpu  = Some(cpu);
        }
    });
    best_cpu
}
