// SPDX-License-Identifier: GPL-2.0
//! Periodic load balancing — gentle nudge to CFS, not a replacement.
use crate::{bindings, state::cpu_state};
use crate::topology::smp;
use crate::runtime;

pub fn run() {
    if !runtime::is_pack_enabled() { return; }

    let (mut max_cpu, mut min_cpu) = (usize::MAX, usize::MAX);
    let (mut max_util, mut min_util) = (0u32, u32::MAX);

    smp::for_each_online(|cpu| {
        let u = cpu_state::util(cpu);
        if u > max_util { max_util = u; max_cpu = cpu; }
        if u < min_util { min_util = u; min_cpu = cpu; }
    });

    if max_cpu == usize::MAX || min_cpu == usize::MAX || max_cpu == min_cpu { return; }
    // Only rebalance on significant imbalance
    if max_util.saturating_sub(min_util) < bindings::PACK_TASK_UTIL_MAX { return; }

    // Log imbalance; actual migration handled by CFS via load_balance()
    let msg = b"balance: imbalance detected";
    unsafe { bindings::arg_shim_log_debug(msg.as_ptr(), msg.len()); }
}
