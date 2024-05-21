pub enum Singleton {
    Unit,
}

#[no_mangle]
pub fn is_unit(s: Singleton) -> bool {
    match s {
        Singleton::Unit => true,
    }
}
