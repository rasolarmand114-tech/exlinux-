// SPDX-License-Identifier: GPL-2.0
//! ARG Scheduler Layer — Rust Core  v1.0
//! Target: Linux 4.19 · ARM64 · Exynos 850
//!
//! "Rust decides. Assembly executes. Hardware is the source of truth."
//!
//! Layout
//! ──────
//!   lib.rs          this file: init/exit, no_mangle hooks, statics
//!   bindings.rs     extern "C" for shim.c + arch/arm64/*.S
//!   allocator.rs    GlobalAlloc → kmalloc/kfree
//!   error.rs        ArgError / ArgResult
//!   runtime.rs      flag accessors
//!   policy/         packing, async, sync, energy, fps
//!   dispatch/       cpu_select, balance, migration
//!   topology/       smp, cluster (feature detection)
//!   state/          task_state, cpu_state
//!   vthread/        worker, pool, split, join

#![no_std]
#![allow(
    dead_code,
    non_snake_case,
    unused_variables,
    non_upper_case_globals
)]
#![feature(lang_items, alloc_error_handler, allocator_api)]

extern crate alloc;

pub mod allocator;
pub mod bindings;
pub mod dispatch;
pub mod error;
pub mod policy;
pub mod runtime;
pub mod state;
pub mod topology;
pub mod vthread;

use core::sync::atomic::{AtomicBool, AtomicI32, Ordering};

// ─── Runtime flags ────────────────────────────────────────────────
// Written by arg_rust_set_* (called from arg_core.c module_param)
// Read by policy layer via runtime.rs
pub(crate) static ASYNC_MODE:      AtomicI32 = AtomicI32::new(1);  // 1=async(gaming)
pub(crate) static PACK_ENABLED:    AtomicI32 = AtomicI32::new(1);  // 1=on
pub(crate) static VTHREAD_ENABLED: AtomicI32 = AtomicI32::new(1);  // 1=on
pub(crate) static ENERGY_MODE:     AtomicI32 = AtomicI32::new(0);  // 0=off
pub(crate) static FPS_MODE:        AtomicI32 = AtomicI32::new(0);  // 0=off

/// Internal readiness gate — prevents any hook from firing before all
/// subsystems finish initializing.
static READY: AtomicBool = AtomicBool::new(false);

// ─── Lifecycle ────────────────────────────────────────────────────

/// Called by `arg_core.c:arg_init()` before hooks are armed.
/// Returns 0 on success, negative errno on failure.
#[no_mangle]
pub extern "C" fn arg_rust_init() -> i32 {
    // 1. Task-state hash table (in C shim memory)
    let rc = unsafe { bindings::arg_shim_ts_init() };
    if rc != 0 {
        return rc;
    }

    // 2. CPU state snapshot
    state::cpu_state::init();

    // 3. Topology (reads CPU capacities + features)
    if let Err(e) = topology::init() {
        unsafe { bindings::arg_shim_ts_destroy() };
        return e.to_errno();
    }

    // 4. Virtual Thread pool (if enabled)
    if runtime::is_vthread_enabled() {
        if let Err(e) = vthread::init() {
            topology::exit();
            unsafe { bindings::arg_shim_ts_destroy() };
            return e.to_errno();
        }
    }

    READY.store(true, Ordering::Release);
    0
}

/// Called by `arg_core.c:arg_exit()` after hooks are disarmed and
/// `synchronize_sched()` has completed.
#[no_mangle]
pub extern "C" fn arg_rust_exit() {
    READY.store(false, Ordering::Release);
    vthread::exit();
    topology::exit();
    unsafe { bindings::arg_shim_ts_destroy() };
}

// ─── Hook entry points ────────────────────────────────────────────
// Called from hook/picknext.c, hook/enqueue.c, hook/wakeup.c, hook/load.c
// All pointers are opaque; never dereferenced in Rust.

/// pick_next hook — may return a task* override or null (CFS decides)
#[no_mangle]
pub extern "C" fn arg_rust_pick_next(rq: *mut bindings::Rq) -> *mut bindings::Task {
    if !READY.load(Ordering::Acquire) {
        return core::ptr::null_mut();
    }
    dispatch::pick_next(rq)
}

/// enqueue hook — task added to a runqueue
#[no_mangle]
pub extern "C" fn arg_rust_enqueue(rq: *mut bindings::Rq, p: *mut bindings::Task) {
    if !READY.load(Ordering::Acquire) {
        return;
    }
    dispatch::on_enqueue(rq, p);
}

/// wakeup hook — task transitioning to RUNNING
#[no_mangle]
pub extern "C" fn arg_rust_wakeup(rq: *mut bindings::Rq, p: *mut bindings::Task) {
    if !READY.load(Ordering::Acquire) {
        return;
    }
    dispatch::on_wakeup(rq, p);
}

/// load-tick hook — refresh per-CPU utilization snapshot
#[no_mangle]
pub extern "C" fn arg_rust_update_load() {
    if !READY.load(Ordering::Acquire) {
        return;
    }
    state::cpu_state::refresh_all();
    dispatch::balance();
}

// ─── CPU hotplug ──────────────────────────────────────────────────

#[no_mangle]
pub extern "C" fn arg_rust_cpu_online(cpu: i32) {
    if READY.load(Ordering::Relaxed) {
        topology::cpu_up(cpu as u32);
        topology::detect_features(cpu as u32);
        state::cpu_state::cpu_up(cpu as usize);
    }
}

#[no_mangle]
pub extern "C" fn arg_rust_cpu_offline(cpu: i32) {
    if READY.load(Ordering::Relaxed) {
        topology::cpu_down(cpu as u32);
        state::cpu_state::cpu_down(cpu as usize);
        unsafe { bindings::arg_shim_pack_reset(cpu) };
    }
}

// ─── Runtime parameter setters/getters ───────────────────────────

#[no_mangle] pub extern "C" fn arg_rust_set_async(v: i32)   { ASYNC_MODE.store(v, Ordering::Relaxed); }
#[no_mangle] pub extern "C" fn arg_rust_set_pack(v: i32)    { PACK_ENABLED.store(v, Ordering::Relaxed); }
#[no_mangle] pub extern "C" fn arg_rust_set_vthread(v: i32) { VTHREAD_ENABLED.store(v, Ordering::Relaxed); }
#[no_mangle] pub extern "C" fn arg_rust_set_energy(v: i32)  { ENERGY_MODE.store(v, Ordering::Relaxed); }
#[no_mangle] pub extern "C" fn arg_rust_set_fps(v: i32)     { FPS_MODE.store(v, Ordering::Relaxed); }
#[no_mangle] pub extern "C" fn arg_rust_get_async()   -> i32 { ASYNC_MODE.load(Ordering::Relaxed) }
#[no_mangle] pub extern "C" fn arg_rust_get_pack()    -> i32 { PACK_ENABLED.load(Ordering::Relaxed) }
#[no_mangle] pub extern "C" fn arg_rust_get_vthread() -> i32 { VTHREAD_ENABLED.load(Ordering::Relaxed) }
#[no_mangle] pub extern "C" fn arg_rust_get_energy()  -> i32 { ENERGY_MODE.load(Ordering::Relaxed) }
#[no_mangle] pub extern "C" fn arg_rust_get_fps()     -> i32 { FPS_MODE.load(Ordering::Relaxed) }

// ─── no_std required items ────────────────────────────────────────

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    // Never unwind in kernel context.
    // WFI saves power if we somehow reach here.
    loop {
        unsafe { core::arch::asm!("wfi", options(nomem, nostack)); }
    }
}

#[alloc_error_handler]
fn oom(_: core::alloc::Layout) -> ! {
    loop {
        unsafe { core::arch::asm!("wfi", options(nomem, nostack)); }
    }
}

#[lang = "eh_personality"]
extern "C" fn eh_personality() {}
