extern crate flipper;

use flipper::Flipper;
use flipper::fsm::led::Led;

fn main() {
    let flipper = Flipper::attach();
    Led::rgb(10, 0, 0);
}
