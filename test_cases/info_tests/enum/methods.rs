pub enum Foo {
    Zero = 0,
    One,
    Two,
    Three,
}

impl Foo {
    // avoid issues with hash in mangled name
    #[export_name = "Foo__to_u32"]
    pub fn to_u32(self) -> u32 {
        self as u32
    }
}
