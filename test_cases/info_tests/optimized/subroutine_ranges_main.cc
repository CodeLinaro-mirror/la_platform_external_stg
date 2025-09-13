int i = 0;

void L1() { }
void R1() { }
void foo(int x, int y);

int main() {
    for (int i = 1; i < 10000; ++i) {
        foo(0, i);
    }
    return 0;
}
