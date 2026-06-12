// SPDX-License-Identifier: GPL-2.0
//! Lightweight error type → Linux errno
use crate::bindings;
#[derive(Debug, Clone, Copy)]
pub enum ArgError { NoMem, Invalid, NoDevice }
pub type Res<T> = Result<T, ArgError>;
impl ArgError {
    pub fn to_errno(self) -> i32 {
        match self {
            Self::NoMem   => -bindings::ENOMEM,
            Self::Invalid => -bindings::EINVAL,
            Self::NoDevice=> -bindings::ENODEV,
        }
    }
}
