#[repr(u8)]
pub enum Pos8 {
    P0(u32) = 0x0,
    P1(u32) = 0x1,
    P127(u32) = 0x7f,
    P128(u32) = 0x80,
    P255(u32) = 0xff,
}

#[repr(u16)]
pub enum Pos16 {
    P0(u32) = 0x0,
    P1(u32) = 0x1,
    P127(u32) = 0x7f,
    P128(u32) = 0x80,
    P255(u32) = 0xff,
    P256(u32) = 0x100,
    P32767(u32) = 0x7fff,
    P32768(u32) = 0x8000,
    P65535(u32) = 0xffff,
}

#[repr(u32)]
pub enum Pos32 {
    P0(u32) = 0x0,
    P1(u32) = 0x1,
    P127(u32) = 0x7f,
    P128(u32) = 0x80,
    P255(u32) = 0xff,
    P256(u32) = 0x100,
    P32767(u32) = 0x7fff,
    P32768(u32) = 0x8000,
    P65535(u32) = 0xffff,
    P65536(u32) = 0x10000,
    P2147483647(u32) = 0x7fffffff,
    P2147483648(u32) = 0x80000000,
    P4294967295(u32) = 0xffffffff,
}

#[repr(u64)]
pub enum Pos64 {
    P0(u32) = 0x0,
    P1(u32) = 0x1,
    P127(u32) = 0x7f,
    P128(u32) = 0x80,
    P255(u32) = 0xff,
    P256(u32) = 0x100,
    P32767(u32) = 0x7fff,
    P32768(u32) = 0x8000,
    P65535(u32) = 0xffff,
    P65536(u32) = 0x10000,
    P2147483647(u32) = 0x7fffffff,
    P2147483648(u32) = 0x80000000,
    P4294967295(u32) = 0xffffffff,
    P4294967296(u32) = 0x100000000,
    P9223372036854775807(u32) = 0x7fffffffffffffff,
    P9223372036854775808(u32) = 0x8000000000000000,
    P18446744073709551615(u32) = 0xffffffffffffffff,
}

#[repr(u128)]
pub enum Pos128 {
    P0(u32) = 0x0,
    P1(u32) = 0x1,
    P127(u32) = 0x7f,
    P128(u32) = 0x80,
    P255(u32) = 0xff,
    P256(u32) = 0x100,
    P32767(u32) = 0x7fff,
    P32768(u32) = 0x8000,
    P65535(u32) = 0xffff,
    P65536(u32) = 0x10000,
    P2147483647(u32) = 0x7fffffff,
    P2147483648(u32) = 0x80000000,
    P4294967295(u32) = 0xffffffff,
    P4294967296(u32) = 0x100000000,
    P9223372036854775807(u32) = 0x7fffffffffffffff,
    P9223372036854775808(u32) = 0x8000000000000000,
    P18446744073709551615(u32) = 0xffffffffffffffff,
    P18446744073709551616(u32) = 0x10000000000000000u128,
    P170141183460469231731687303715884105727(u32) = 0x7fffffffffffffffffffffffffffffffu128,
    P170141183460469231731687303715884105728(u32) = 0x80000000000000000000000000000000u128,
    P340282366920938463463374607431768211455(u32) = 0xffffffffffffffffffffffffffffffffu128,
}

// LLVM DWARF signedness of these constants is currently ambiguous
#[repr(i8)]
pub enum Neg8 {
    N0(u32) = -0x0,
    N1(u32) = -0x1,
    N127(u32) = -0x7f,
    N128(u32) = -0x80,
}

// LLVM DWARF signedness of these constants is currently ambiguous
#[repr(i16)]
pub enum Neg16 {
    N0(u32) = -0x0,
    N1(u32) = -0x1,
    N127(u32) = -0x7f,
    N128(u32) = -0x80,
    N255(u32) = -0xff,
    N256(u32) = -0x100,
    N32767(u32) = -0x7fff,
    N32768(u32) = -0x8000,
}

// LLVM DWARF signedness of these constants is currently ambiguous
#[repr(i32)]
pub enum Neg32 {
    N0(u32) = -0x0,
    N1(u32) = -0x1,
    N127(u32) = -0x7f,
    N128(u32) = -0x80,
    N255(u32) = -0xff,
    N256(u32) = -0x100,
    N32767(u32) = -0x7fff,
    N32768(u32) = -0x8000,
    N65535(u32) = -0xffff,
    N65536(u32) = -0x10000,
    N2147483647(u32) = -0x7fffffff,
    N2147483648(u32) = -0x80000000,
}

// LLVM DWARF signedness of these constants is currently ambiguous
#[repr(i64)]
pub enum Neg64 {
    N0(u32) = -0x0,
    N1(u32) = -0x1,
    N127(u32) = -0x7f,
    N128(u32) = -0x80,
    N255(u32) = -0xff,
    N256(u32) = -0x100,
    N32767(u32) = -0x7fff,
    N32768(u32) = -0x8000,
    N65535(u32) = -0xffff,
    N65536(u32) = -0x10000,
    N2147483647(u32) = -0x7fffffff,
    N2147483648(u32) = -0x80000000,
    N4294967295(u32) = -0xffffffff,
    N4294967296(u32) = -0x100000000,
    N9223372036854775807(u32) = -0x7fffffffffffffff,
    N9223372036854775808(u32) = -0x8000000000000000,
}

#[repr(i128)]
pub enum Neg128 {
    N0(u32) = -0x0,
    N1(u32) = -0x1,
    N127(u32) = -0x7f,
    N128(u32) = -0x80,
    N255(u32) = -0xff,
    N256(u32) = -0x100,
    N32767(u32) = -0x7fff,
    N32768(u32) = -0x8000,
    N65535(u32) = -0xffff,
    N65536(u32) = -0x10000,
    N2147483647(u32) = -0x7fffffff,
    N2147483648(u32) = -0x80000000,
    N4294967295(u32) = -0xffffffff,
    N4294967296(u32) = -0x100000000,
    N9223372036854775807(u32) = -0x7fffffffffffffff,
    N9223372036854775808(u32) = -0x8000000000000000,
    N18446744073709551615(u32) = -0xffffffffffffffffi128,
    N18446744073709551616(u32) = -0x10000000000000000i128,
    N170141183460469231731687303715884105727(u32) = -0x7fffffffffffffffffffffffffffffffi128,
    N170141183460469231731687303715884105728(u32) = -0x80000000000000000000000000000000i128,
}

pub fn use_pos(_pos8: Pos8, _pos16: Pos16, _pos32: Pos32, _pos64: Pos64, _pos128: Pos128) {}
pub fn use_neg(_neg8: Neg8, _neg16: Neg16, _neg32: Neg32, _neg64: Neg64, _neg128: Neg128) {}
