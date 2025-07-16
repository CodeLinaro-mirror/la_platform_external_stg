#[repr(u64)]
pub enum Pos {
    A(u32) = 0,
    B(u32) = 1,
    C(u32) = 0x7f,
    D(u32) = 0x80,
    E(u32) = 0xff,
    F(u32) = 0x100,
    G(u32) = 0x7fff,
    H(u32) = 0x8000,
    I(u32) = 0xffff,
    J(u32) = 0x10000,
    K(u32) = 0x7fffffff,
    L(u32) = 0x80000000,
    M(u32) = 0xffffffff,
    N(u32) = 0x100000000,
    O(u32) = 0x7fffffffffffffff,
    // P(u32) = 0x8000000000000000,
    // Q(u32) = 0xffffffffffffffff,
}

// LLVM DWARF signedness of these constants is currently ambiguous
#[repr(i64)]
pub enum Neg {
    A(u32) = -0,
    B(u32) = -1,
    C(u32) = -0x80,
    D(u32) = -0x81,
    E(u32) = -0x100,
    F(u32) = -0x101,
    G(u32) = -0x8000,
    H(u32) = -0x8001,
    I(u32) = -0x10000,
    J(u32) = -0x10001,
    K(u32) = -0x80000000,
    L(u32) = -0x80000001,
    M(u32) = -0x100000000,
    N(u32) = -0x100000001,
    O(u32) = -0x8000000000000000,
    // P(u32) = -0x8000000000000001,
}

pub fn use_pos_neg(_pos: Pos, _neg: Neg) {}
