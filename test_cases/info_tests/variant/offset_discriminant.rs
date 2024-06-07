pub enum Foo {
    Two(char, char),
    One(char),
    Zero,
}

#[no_mangle]
pub fn is_zero(foo: Foo) -> bool {
    match foo {
        Foo::Zero => true,
        _ => false,
    }
}
