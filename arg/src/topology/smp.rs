// SPDX-License-Identifier: GPL-2.0
//! SMP topology: online mask, capacity cache
use core::sync::atomic::{AtomicBool, AtomicU32, Ordering::*};
use crate::bindings;
use crate::error::Res;

const N: usize = bindings::MAX_CPU;
static ONLINE: [AtomicBool; N] = [AtomicBool::new(false); N];
static CAP:    [AtomicU32;  N] = [AtomicU32::new(0);      N];

pub fn init() -> Res<()> {
    let nr = unsafe { bindings::arg_shim_nr_cpus() } as usize;
    for cpu in 0..nr.min(N) {
        let online = unsafe { bindings::arg_shim_cpu_online(cpu as i32) };
        ONLINE[cpu].store(online, Relaxed);
        if online {
            let c = unsafe { bindings::arg_shim_cpu_capacity(cpu as i32) };
            CAP[cpu].store(c, Relaxed);
        }
    }
    Ok(())
}

pub fn mark_online(cpu: usize, v: bool) {
    if cpu < N {
        ONLINE[cpu].store(v, Relaxed);
        if v {
            let c = unsafe { bindings::arg_shim_cpu_capacity(cpu as i32) };
            CAP[cpu].store(c, Relaxed);
        }
    }
}

#[inline] pub fn is_online(cpu: usize) -> bool { cpu < N && ONLINE[cpu].load(Relaxed) }
#[inline] pub fn capacity(cpu: usize)  -> u32  { if cpu < N { CAP[cpu].load(Relaxed) } else { 0 } }

/// Iterate over all online CPUs (0-indexed)
pub fn for_each_online(mut f: impl FnMut(usize)) {
    for cpu in 0..N {
        if ONLINE[cpu].load(Relaxed) { f(cpu); }
    }
}
