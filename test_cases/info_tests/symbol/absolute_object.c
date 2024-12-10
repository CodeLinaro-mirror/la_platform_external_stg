// Create ELF symbol that is global object and has absolute value
__asm__(
    ".global bar\n"
    ".type bar,object\n"
    ".size bar,0\n"
    "bar = 0\n");

long x, y;
