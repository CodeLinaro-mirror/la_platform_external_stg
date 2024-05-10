#[repr(i64)]
pub enum Foo {
    MinusTwo(u32) = -2,
    MinusOne = -1,
    Zero(u32),
}

#[no_mangle]
pub fn is_minus_one(foo: Foo) -> bool {
    match foo {
        Foo::MinusOne => true,
        _ => false,
    }
}
