extern int i;

void L1();
void R1();

void foo(int x, int y) {
    if (x == 0) {
        L1();
    } else {
        R1();
        y += 1;
    }
    i += y;
}
