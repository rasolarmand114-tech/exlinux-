// SPDX-License-Identifier: GPL-2.0
//! VThread worker: a kthread pinned to a single CPU.
use crate::bindings::{self, Task};

pub struct Worker {
    pub cpu:    usize,
    pub thread: *mut Task,   // opaque kthread handle
    pub load:   u32,         // share of parent task load
}

// Safety: Worker is always accessed from one CPU at a time.
unsafe impl Send for Worker {}
unsafe impl Sync for Worker {}

impl Worker {
    pub fn new(cpu: usize, load: u32) -> Option<Self> {
        let name = b"arg_vt\0";
        let thread = unsafe {
            bindings::arg_shim_kthread_create_cpu(cpu as i32, name.as_ptr())
        };
        if thread.is_null() { return None; }
        Some(Worker { cpu, thread, load })
    }

    pub fn wake(&self) {
        if !self.thread.is_null() {
            unsafe { bindings::arg_shim_kthread_wake(self.thread); }
        }
    }
}

impl Drop for Worker {
    fn drop(&mut self) {
        if !self.thread.is_null() {
            unsafe { bindings::arg_shim_kthread_stop(self.thread); }
            self.thread = core::ptr::null_mut();
        }
    }
}
