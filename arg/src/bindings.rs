// SPDX-License-Identifier: GPL-2.0
//! src/bindings.rs — FFI surface: shim.c + arch/arm64/*.S
//! Rust never dereferences these pointers directly.

#![allow(non_camel_case_types, dead_code)]

// Opaque kernel handles
#[repr(C)] pub struct Task { _p: [u8; 0] }
#[repr(C)] pub struct Rq   { _p: [u8; 0] }
#[repr(C)] pub struct Ts   { _p: [u8; 0] }   // arg_ts in shim.c

// errno
pub const ENOMEM: i32 = 12;
pub const EINVAL: i32 = 22;
pub const ENODEV: i32 = 19;

// Sched policy
pub const SCHED_FIFO:     u32 = 1;
pub const SCHED_RR:       u32 = 2;
pub const SCHED_DEADLINE: u32 = 6;

// GFP flags
pub const GFP_KERNEL: u32 = 0x0014_00c0;
pub const GFP_ATOMIC: u32 = 0x0000_0400;

// Capacity scale
pub const CAP_SCALE: u32 = 1024;

// ARG thresholds
pub const LARGE_TASK_UTIL:    u32 = 512;
pub const PACK_TASK_UTIL_MAX: u32 = 102;   // ~10%
pub const PACK_BIN_UTIL_MAX:  u32 = 819;   // ~80%
pub const BIG_CORE_CAP_THR:   u32 = 800;

pub const MAX_CPU: usize = 8;   // Exynos 850: 8× Cortex-A55

extern "C" {
    // Task
    pub fn arg_shim_task_util(p: *mut Task) -> u32;
    pub fn arg_shim_task_cpu(p: *mut Task) -> i32;
    pub fn arg_shim_task_policy(p: *mut Task) -> u32;
    pub fn arg_shim_task_pid(p: *mut Task) -> i32;
    pub fn arg_shim_task_is_rt(p: *mut Task) -> bool;
    pub fn arg_shim_task_is_dl(p: *mut Task) -> bool;
    pub fn arg_shim_task_cpu_allowed(p: *mut Task, cpu: i32) -> i32;
    // Rq
    pub fn arg_shim_cpu_rq(cpu: i32) -> *mut Rq;
    pub fn arg_shim_rq_cpu(rq: *mut Rq) -> i32;
    pub fn arg_shim_rq_nr_running(rq: *mut Rq) -> u32;
    pub fn arg_shim_rq_rt_nr(rq: *mut Rq) -> u32;
    // CPU
    pub fn arg_shim_cpu_util(cpu: i32) -> u32;
    pub fn arg_shim_cpu_capacity(cpu: i32) -> u32;
    pub fn arg_shim_cpu_idle(cpu: i32) -> bool;
    pub fn arg_shim_cpu_online(cpu: i32) -> bool;
    pub fn arg_shim_nr_cpus() -> i32;
    pub fn arg_shim_this_cpu() -> i32;
    // Memory
    pub fn arg_shim_kmalloc(size: usize, flags: u32) -> *mut u8;
    pub fn arg_shim_kfree(ptr: *mut u8);
    pub fn arg_shim_ktime_ns() -> u64;
    pub fn arg_shim_sync_sched();
    // Task-state hash table
    pub fn arg_shim_ts_init() -> i32;
    pub fn arg_shim_ts_destroy();
    pub fn arg_shim_ts_get(task: *mut Task) -> *mut Ts;
    pub fn arg_shim_ts_create(task: *mut Task) -> *mut Ts;
    pub fn arg_shim_ts_remove(task: *mut Task);
    pub fn arg_shim_ts_get_util(ts: *mut Ts) -> u32;
    pub fn arg_shim_ts_set_util(ts: *mut Ts, v: u32);
    pub fn arg_shim_ts_get_large(ts: *mut Ts) -> u8;
    pub fn arg_shim_ts_set_large(ts: *mut Ts, v: u8);
    pub fn arg_shim_ts_get_mig(ts: *mut Ts) -> u32;
    pub fn arg_shim_ts_inc_mig(ts: *mut Ts);
    pub fn arg_shim_ts_get_enq(ts: *mut Ts) -> u64;
    pub fn arg_shim_ts_set_enq(ts: *mut Ts, v: u64);
    pub fn arg_shim_ts_get_wake(ts: *mut Ts) -> u64;
    pub fn arg_shim_ts_set_wake(ts: *mut Ts, v: u64);
    // Pack bins
    pub fn arg_shim_pack_get(cpu: i32) -> u32;
    pub fn arg_shim_pack_add(cpu: i32, util: u32);
    pub fn arg_shim_pack_sub(cpu: i32, util: u32);
    pub fn arg_shim_pack_reset(cpu: i32);
    // kthread
    pub fn arg_shim_kthread_create_cpu(cpu: i32, name: *const u8) -> *mut Task;
    pub fn arg_shim_kthread_stop(t: *mut Task);
    pub fn arg_shim_kthread_wake(t: *mut Task);
    // SMP
    pub fn arg_shim_smp_call(cpu: i32, f: unsafe extern "C" fn(*mut core::ffi::c_void), a: *mut core::ffi::c_void, wait: i32);
    // Log
    pub fn arg_shim_log_info(ptr: *const u8, len: usize);
    pub fn arg_shim_log_debug(ptr: *const u8, len: usize);
    // ── ARM64 assembly (arch/arm64/*.S) ──
    pub fn arg_arm64_dmb_ish();
    pub fn arg_arm64_dsb_ish();
    pub fn arg_arm64_isb();
    pub fn arg_arm64_prefetch_r(addr: *const u8);
    pub fn arg_arm64_prefetch_w(addr: *const u8);
    pub fn arg_arm64_dc_clean(start: *mut u8, len: usize);
    pub fn arg_arm64_dc_inval(start: *mut u8, len: usize);
    pub fn arg_arm64_tlb_flush();
    pub fn arg_arm64_read_pmccntr() -> u64;
    pub fn arg_arm64_read_mpidr() -> u64;
    pub fn arg_arm64_read_isar0() -> u64;
    pub fn arg_arm64_read_pfr0() -> u64;
    pub fn arg_arm64_lse_add(ptr: *mut u32, val: u32) -> u32;
    pub fn arg_arm64_lse_cas(ptr: *mut u32, old: u32, new: u32) -> u32;
    pub fn arg_arm64_lse_swap(ptr: *mut u32, new: u32) -> u32;
}
