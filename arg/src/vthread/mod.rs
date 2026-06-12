// SPDX-License-Identifier: GPL-2.0
//! Virtual Thread Layer
//! Decomposes large tasks across independent per-CPU workers.
//! "One Thread → One CPU At A Time" — each worker owns its CPU.
pub mod join;
pub mod pool;
pub mod split;
pub mod worker;

use crate::error::Res;

pub fn init()  -> Res<()> { pool::init() }
pub fn exit()            { pool::drain(); }
