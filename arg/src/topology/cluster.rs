// SPDX-License-Identifier: GPL-2.0
//! ARM64 per-CPU feature cache.
//! Features are detected ONCE via MRS (from regs.S) and never re-read.
//! "Feature Detection is a Rust responsibility." — AOKL spec.
use core::sync::atomic::{AtomicBool, AtomicU64, Ordering::*};
use crate::bindings;

const N: usize = bindings::MAX_CPU;

// Each feature gets its own per-CPU bool array
static DETECTED: [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_LSE:  [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_FP:   [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_SVE:  [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_CRC:  [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_AES:  [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_SHA1: [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_SHA2: [AtomicBool; N] = [AtomicBool::new(false); N];
static HAS_PMUL: [AtomicBool; N] = [AtomicBool::new(false); N];
static MPIDR:    [AtomicU64;  N] = [AtomicU64::new(0);      N];

/// Read ISAR0 + PFR0 on this CPU and cache the results.
/// Called on the target CPU via smp_call_function_single (from detect).
extern "C" fn do_detect(_: *mut core::ffi::c_void) {
    let cpu = unsafe { bindings::arg_shim_this_cpu() } as usize;
    if cpu >= N || DETECTED[cpu].load(Relaxed) { return; }

    let isar0 = unsafe { bindings::arg_arm64_read_isar0() };
    let pfr0  = unsafe { bindings::arg_arm64_read_pfr0() };
    let mpidr = unsafe { bindings::arg_arm64_read_mpidr() };

    // ID_AA64ISAR0_EL1 field extraction
    HAS_AES[cpu].store((isar0 >>  4) & 0xF >= 1, Relaxed);
    HAS_SHA1[cpu].store((isar0 >> 8) & 0xF >= 1, Relaxed);
    HAS_SHA2[cpu].store((isar0 >>12) & 0xF >= 1, Relaxed);
    HAS_CRC[cpu].store((isar0  >>16) & 0xF >= 1, Relaxed);
    HAS_LSE[cpu].store((isar0  >>20) & 0xF >= 2, Relaxed); // FEAT_LSE = value 2
    HAS_PMUL[cpu].store((isar0 >>48) & 0xF >= 1, Relaxed);

    // ID_AA64PFR0_EL1: FP field 0xF = not present
    HAS_FP[cpu].store((pfr0  >>16) & 0xF != 0xF, Relaxed);
    HAS_SVE[cpu].store((pfr0 >>32) & 0xF >= 1, Relaxed);

    MPIDR[cpu].store(mpidr, Relaxed);
    DETECTED[cpu].store(true, Release);
}

pub fn detect(cpu: usize) {
    if cpu >= N || DETECTED[cpu].load(Relaxed) { return; }
    let this = unsafe { bindings::arg_shim_this_cpu() } as usize;
    if this == cpu {
        do_detect(core::ptr::null_mut());
    } else {
        unsafe {
            bindings::arg_shim_smp_call(
                cpu as i32, do_detect, core::ptr::null_mut(), 1);
        }
    }
}

pub fn detect_all_online() {
    for cpu in 0..N {
        if crate::topology::smp::is_online(cpu) { detect(cpu); }
    }
}

#[inline] pub fn has_lse(cpu: usize)  -> bool { cpu < N && HAS_LSE[cpu].load(Relaxed) }
#[inline] pub fn has_fp(cpu: usize)   -> bool { cpu < N && HAS_FP[cpu].load(Relaxed)  }
#[inline] pub fn has_sve(cpu: usize)  -> bool { cpu < N && HAS_SVE[cpu].load(Relaxed) }
#[inline] pub fn mpidr(cpu: usize)    -> u64  { if cpu < N { MPIDR[cpu].load(Relaxed) } else { 0 } }
/// Aff2 field of MPIDR — cluster ID (big.LITTLE detection)
#[inline] pub fn cluster_id(cpu: usize) -> u8 { ((mpidr(cpu) >> 16) & 0xFF) as u8 }
