#[repr(u64)]
pub enum Pos {
    A = 0,
    B = 1,
    C = 0x7f,
    D = 0x80,
    E = 0xff,
    F = 0x100,
    G = 0x7fff,
    H = 0x8000,
    I = 0xffff,
    J = 0x10000,
    K = 0x7fffffff,
    L = 0x80000000,
    M = 0xffffffff,
    N = 0x100000000,
    O = 0x7fffffffffffffff,
    // P = 0x8000000000000000,
    // Q = 0xffffffffffffffff,
}

#[repr(i64)]
pub enum Neg {
    A = -0,
    B = -1,
    C = -0x80,
    D = -0x81,
    E = -0x100,
    F = -0x101,
    G = -0x8000,
    H = -0x8001,
    I = -0x10000,
    J = -0x10001,
    K = -0x80000000,
    L = -0x80000001,
    M = -0x100000000,
    N = -0x100000001,
    O = -0x8000000000000000,
    // P = -0x8000000000000001,
}

pub fn use_pos_neg(_pos: Pos, _neg: Neg) {}
