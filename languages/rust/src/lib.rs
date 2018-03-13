#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![deny(unstable_features)]
#![deny(unused_import_braces)]
#![deny(unused_qualifications)]
//#![deny(warnings)]

extern crate libc;

pub mod fsm;
pub mod lf;

use libc::{c_void, c_char};
use std::ffi::CString;

type _lf_device = *const c_void;
type _lf_function_index = u8;
type _fmr_return = u32;

pub const LF_VERSION: u16 = 0x0001;

#[link(name = "flipper")]
extern {
    fn flipper_attach() -> _lf_device;
    fn carbon_attach_hostname(hostname: *const c_char) -> _lf_device;
    fn lf_get_current_device() -> _lf_device;
}

pub struct Flipper {
    /// A reference to an active Flipper profile in libflipper. This
    /// is used when communicating with libflipper to specify which
    /// device functions should be executed on.
    device: _lf_device
}

impl Flipper {

    pub fn attach() -> Self {
        unsafe {
            Flipper { device: flipper_attach() }
        }
    }

    pub fn attach_hostname(hostname: &str) -> Self {
        unsafe {
            Flipper { device: carbon_attach_hostname(CString::new(hostname).unwrap().as_ptr()) }
        }
    }
}
