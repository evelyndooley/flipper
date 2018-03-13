#![allow(non_upper_case_globals)]

use lf;
use std::io::{Read, Write, Result};

pub enum UartBaud {
    FMR,
    DFU,
}

impl UartBaud {
    fn to_baud(&self) -> u8 {
        match *self {
            UartBaud::FMR => 0x00,
            UartBaud::DFU => 0x10,
        }
    }
}

pub struct Uart0 {

}

impl Uart0 {
    pub fn new() -> Self {
        Uart0 { }
    }

    /// Configures the Uart0 module with a given baud rate and
    /// interrupts enabled flag.
    pub fn configure(&self, baud: &UartBaud, interrupts: bool) {
        let args = lf::Args::new()
            .append(baud.to_baud())
            .append(if interrupts { 1u8 } else { 0u8 });
        lf::invoke("uart0", 0, Some(args))
    }

    /// Indicates whether the Uart0 bus is ready to read or write.
    pub fn ready(&self) -> bool {
        let ret: u8 = lf::invoke("uart0", 1, None);
        ret != 0
    }
}

impl Write for Uart0 {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        lf::push::<()>("uart0", 2, buf, None);
        Ok(buf.len())
    }
    fn flush(&mut self) -> Result<()> { Ok(()) }
}

impl Read for Uart0 {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        lf::pull::<()>("uart0", 3, buf, None);
        Ok(buf.len())
    }
}
