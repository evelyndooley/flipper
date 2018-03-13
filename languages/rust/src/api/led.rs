use lf;
use lf::_lf_device;

pub struct Led {
    device: _lf_device
}

impl Led {
    pub fn new(device: _lf_device) -> Self {
        Led { device: device }
    }

    pub fn configure(self) {
        lf::invoke(self.device, "led", 1, None)
    }

    pub fn rgb(self, r: u8, g: u8, b: u8) {
        let args = lf::Args::new().append(r).append(g).append(b);
        lf::invoke(self.device, "led", 0, Some(args))
    }
}
