use lf;

pub struct Led;

impl Led {
    pub fn configure() {
        lf::invoke("led", 1, None)
    }

    pub fn rgb(r: u8, g: u8, b: u8) {
        let args = lf::Args::new().append(r).append(g).append(b);
        lf::invoke("led", 0, Some(args))
    }
}
