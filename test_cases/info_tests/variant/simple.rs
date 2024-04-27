#[repr(u8)]
pub enum Foo {
    Unit,
    TwoU32s(u32, u32),
    ThreeI16s { x: i16, y: i16, z: i16 },
}

#[no_mangle]
pub fn is_unit(foo: Foo) -> bool {
    match foo {
        Foo::Unit => true,
        _ => false,
    }
}
