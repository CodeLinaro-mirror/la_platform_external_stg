int x = 0;
int y = 1;
extern int z __attribute__ ((weak, alias ("x")));

int a() { return 0; }
int b() { return 1; }
extern int c() __attribute__ ((alias ("b")));
