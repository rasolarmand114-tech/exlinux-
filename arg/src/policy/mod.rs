// SPDX-License-Identifier: GPL-2.0
//! Policy layer — scheduling rules.
//! Dispatch calls the active policy; policy returns best CPU or None.
pub mod async_mode;
pub mod energy;
pub mod fps;
pub mod packing;
pub mod sync_mode;
