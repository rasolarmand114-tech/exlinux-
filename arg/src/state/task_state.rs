// SPDX-License-Identifier: GPL-2.0
//! Task-state: thin Rust wrapper over the opaque arg_ts in shim.c
use crate::bindings::{self, Task, Ts};

/// Get or create the state record for task p.
/// Returns null if task is RT/DL (we never track those).
pub fn get_or_create(p: *mut Task) -> *mut Ts {
    if p.is_null() { return core::ptr::null_mut(); }
    if unsafe { bindings::arg_shim_task_is_rt(p) || bindings::arg_shim_task_is_dl(p) } {
        return core::ptr::null_mut();
    }
    let ts = unsafe { bindings::arg_shim_ts_get(p) };
    if !ts.is_null() { return ts; }
    unsafe { bindings::arg_shim_ts_create(p) }
}

pub fn update_enqueue(p: *mut Task) {
    let ts = get_or_create(p);
    if ts.is_null() { return; }
    let util = unsafe { bindings::arg_shim_task_util(p) };
    let now  = unsafe { bindings::arg_shim_ktime_ns() };
    unsafe {
        bindings::arg_shim_ts_set_util(ts, util);
        bindings::arg_shim_ts_set_large(ts, (util > bindings::LARGE_TASK_UTIL) as u8);
        bindings::arg_shim_ts_set_enq(ts, now);
    }
}

pub fn update_wakeup(p: *mut Task) {
    let ts = get_or_create(p);
    if ts.is_null() { return; }
    let now = unsafe { bindings::arg_shim_ktime_ns() };
    unsafe { bindings::arg_shim_ts_set_wake(ts, now); }
}

pub fn task_util(p: *mut Task) -> u32 {
    unsafe { bindings::arg_shim_task_util(p) }
}
pub fn is_large(p: *mut Task) -> bool {
    task_util(p) > bindings::LARGE_TASK_UTIL
}
pub fn is_rt(p: *mut Task) -> bool {
    unsafe { bindings::arg_shim_task_is_rt(p) || bindings::arg_shim_task_is_dl(p) }
}
