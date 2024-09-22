pub enum Empty {}

#[no_mangle]
pub fn is_none(opt: Option<Empty>) -> bool {
    match opt {
        None => true,
    }
}
