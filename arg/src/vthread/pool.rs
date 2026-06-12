// SPDX-License-Identifier: GPL-2.0
//! VThread pool — at most one worker per CPU.
use crate::error::{ArgError, Res};
use crate::vthread::worker::Worker;
use crate::bindings::MAX_CPU;

// Fixed-size pool: one optional Worker per CPU
// Uses raw pointers + Manual resource management to avoid Mutex in no_std.
static mut POOL: [Option<Worker>; MAX_CPU] = [
    None, None, None, None, None, None, None, None,
];

pub fn init() -> Res<()> {
    // Pool starts empty; workers created on demand by split.rs
    Ok(())
}

pub fn drain() {
    unsafe {
        for slot in POOL.iter_mut() {
            *slot = None;  // Drop impl calls kthread_stop
        }
    }
}

/// Get or create a worker for `cpu` with `load` share.
pub fn get_or_create(cpu: usize, load: u32) -> Option<&'static Worker> {
    if cpu >= MAX_CPU { return None; }
    unsafe {
        if POOL[cpu].is_none() {
            POOL[cpu] = Worker::new(cpu, load);
        }
        POOL[cpu].as_ref()
    }
}

pub fn get(cpu: usize) -> Option<&'static Worker> {
    if cpu >= MAX_CPU { return None; }
    unsafe { POOL[cpu].as_ref() }
}
