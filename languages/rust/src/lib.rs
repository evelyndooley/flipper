#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![deny(unstable_features)]
#![deny(unused_import_braces)]
#![deny(unused_qualifications)]
//#![deny(warnings)]

extern crate libc;

pub mod api;
pub mod lf;

use libc::c_char;
use std::ffi::CString;
use lf::_lf_device;

pub const LF_VERSION: u16 = 0x0001;

#[link(name = "flipper")]
extern {
    fn flipper_attach() -> _lf_device;
    fn carbon_attach_hostname(hostname: *const c_char) -> _lf_device;
}


pub struct Flipper;

impl Flipper {
    pub fn attach() -> _lf_device {
        unsafe {
            flipper_attach()
        }
    }

    pub fn attach_hostname(hostname: &str) -> _lf_device {
        unsafe {
            carbon_attach_hostname(CString::new(hostname).unwrap().as_ptr())
        }
    }
}
