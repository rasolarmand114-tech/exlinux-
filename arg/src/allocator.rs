// SPDX-License-Identifier: GPL-2.0
//! Kernel kmalloc/kfree as Rust GlobalAlloc (GFP_ATOMIC — no sleep)
use core::alloc::{GlobalAlloc, Layout};
use crate::bindings;

pub struct KernelAlloc;
unsafe impl GlobalAlloc for KernelAlloc {
    unsafe fn alloc(&self, l: Layout) -> *mut u8 {
        bindings::arg_shim_kmalloc(l.size(), bindings::GFP_ATOMIC)
    }
    unsafe fn dealloc(&self, p: *mut u8, _: Layout) {
        if !p.is_null() { bindings::arg_shim_kfree(p); }
    }
}
#[global_allocator]
pub static A: KernelAlloc = KernelAlloc;
