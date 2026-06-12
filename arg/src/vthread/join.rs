// SPDX-License-Identifier: GPL-2.0
//! Join: collect worker results.  Currently a stub because the actual
//! work-item protocol is application-defined above ARG's scope.
//! ARG only guarantees the workers are running on their pinned CPUs.
pub struct JoinResult {
    pub workers_active: usize,
}

pub fn join_all() -> JoinResult {
    use crate::{bindings::MAX_CPU, vthread::pool};
    let mut active = 0;
    for cpu in 0..MAX_CPU {
        if pool::get(cpu).is_some() { active += 1; }
    }
    JoinResult { workers_active: active }
}
