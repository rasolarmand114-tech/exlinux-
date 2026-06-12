// SPDX-License-Identifier: GPL-2.0
//! Topology layer: SMP + big.LITTLE feature detection
//! "Feature Detection" is a Rust responsibility per AOKL.
pub mod cluster;
pub mod smp;

use crate::error::{ArgError, Res};
use crate::bindings;

pub fn init() -> Res<()> {
    smp::init()?;
    cluster::detect_all_online();
    Ok(())
}
pub fn exit() { /* static data, nothing to free */ }
pub fn cpu_up(cpu: u32)   { smp::mark_online(cpu as usize, true);  cluster::detect(cpu as usize); }
pub fn cpu_down(cpu: u32) { smp::mark_online(cpu as usize, false); }
pub fn detect_features(cpu: u32) { cluster::detect(cpu as usize); }
