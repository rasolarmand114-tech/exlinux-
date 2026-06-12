// SPDX-License-Identifier: GPL-2.0
//! Per-CPU state — utilization, capacity, idle, big-core flag.
//! All fields are AtomicU32/AtomicBool → lock-free reads from scheduler.
use core::sync::atomic::{AtomicBool, AtomicU32, Ordering::*};
use crate::bindings;

const N: usize = bindings::MAX_CPU;

static UTIL:     [AtomicU32;  N] = [AtomicU32::new(0);   N];
static CAP:      [AtomicU32;  N] = [AtomicU32::new(1024); N];
static IS_IDLE:  [AtomicBool; N] = [AtomicBool::new(true);  N];
static IS_BIG:   [AtomicBool; N] = [AtomicBool::new(false); N];
static IS_ONLINE:[AtomicBool; N] = [AtomicBool::new(false); N];

pub fn init() {
    let nr = unsafe { bindings::arg_shim_nr_cpus() } as usize;
    for cpu in 0..nr.min(N) {
        if unsafe { bindings::arg_shim_cpu_online(cpu as i32) } {
            refresh(cpu);
            IS_ONLINE[cpu].store(true, Relaxed);
        }
    }
}

pub fn refresh(cpu: usize) {
    if cpu >= N { return; }
    let u = unsafe { bindings::arg_shim_cpu_util(cpu as i32) };
    let c = unsafe { bindings::arg_shim_cpu_capacity(cpu as i32) };
    let i = unsafe { bindings::arg_shim_cpu_idle(cpu as i32) };
    UTIL[cpu].store(u, Relaxed);
    CAP[cpu].store(c, Relaxed);
    IS_IDLE[cpu].store(i, Relaxed);
    IS_BIG[cpu].store(c >= bindings::BIG_CORE_CAP_THR, Relaxed);
}

pub fn refresh_all() {
    let nr = unsafe { bindings::arg_shim_nr_cpus() } as usize;
    for cpu in 0..nr.min(N) {
        if IS_ONLINE[cpu].load(Relaxed) { refresh(cpu); }
    }
}

pub fn cpu_up(cpu: usize) {
    if cpu < N { IS_ONLINE[cpu].store(true, Relaxed); refresh(cpu); }
}
pub fn cpu_down(cpu: usize) {
    if cpu < N {
        IS_ONLINE[cpu].store(false, Relaxed);
        UTIL[cpu].store(0, Relaxed);
        IS_IDLE[cpu].store(true, Relaxed);
    }
}

#[inline] pub fn util(cpu: usize)     -> u32  { if cpu < N { UTIL[cpu].load(Relaxed) } else { 0 } }
#[inline] pub fn cap(cpu: usize)      -> u32  { if cpu < N { CAP[cpu].load(Relaxed)  } else { 0 } }
#[inline] pub fn is_idle(cpu: usize)  -> bool { cpu < N && IS_IDLE[cpu].load(Relaxed) }
#[inline] pub fn is_big(cpu: usize)   -> bool { cpu < N && IS_BIG[cpu].load(Relaxed) }
#[inline] pub fn is_online(cpu: usize)-> bool { cpu < N && IS_ONLINE[cpu].load(Relaxed) }
#[inline] pub fn headroom(cpu: usize) -> u32  {
    let u = util(cpu);
    let c = bindings::PACK_BIN_UTIL_MAX;
    if u < c { c - u } else { 0 }
}
