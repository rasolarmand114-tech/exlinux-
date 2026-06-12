// SPDX-License-Identifier: GPL-2.0
//! Runtime flag accessors (read by policy/dispatch)
use core::sync::atomic::Ordering::Relaxed;
use crate::{ASYNC_MODE,PACK_ENABLED,VTHREAD_ENABLED,ENERGY_MODE,FPS_MODE};
#[inline] pub fn is_async()           -> bool { ASYNC_MODE.load(Relaxed)      != 0 }
#[inline] pub fn is_pack_enabled()    -> bool { PACK_ENABLED.load(Relaxed)    != 0 }
#[inline] pub fn is_vthread_enabled() -> bool { VTHREAD_ENABLED.load(Relaxed) != 0 }
#[inline] pub fn is_energy_mode()     -> bool { ENERGY_MODE.load(Relaxed)     != 0 }
#[inline] pub fn is_fps_mode()        -> bool { FPS_MODE.load(Relaxed)        != 0 }
