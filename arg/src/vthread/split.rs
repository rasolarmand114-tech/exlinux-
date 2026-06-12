// SPDX-License-Identifier: GPL-2.0
//! Work splitting: decompose a large task's load across idle CPUs.
//! Each slice is handled by a separate Worker — no shared context.
use crate::{bindings, state::cpu_state, topology::smp};
use crate::vthread::pool;

/// Try to distribute `total_util` across available idle CPUs.
/// Returns number of workers activated.
pub fn split(total_util: u32) -> usize {
    let mut remaining = total_util;
    let mut activated = 0usize;

    smp::for_each_online(|cpu| {
        if remaining == 0 { return; }
        if !cpu_state::is_idle(cpu) { return; }

        let slice = remaining.min(bindings::LARGE_TASK_UTIL / 2);
        if let Some(w) = pool::get_or_create(cpu, slice) {
            w.wake();
            remaining = remaining.saturating_sub(slice);
            activated += 1;
        }
    });
    activated
}
