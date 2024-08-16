// tweaked due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112372
int u(unsigned char c) { (void)c; return 0; }
int v(signed char c) { (void)c; return 1; }
int w(char c) { (void)c; return 2; }
int x(signed char c) { (void)c; return 3; }
int y(char c) { (void)c; return 4; }
int z(unsigned char c) { (void)c; return 5; }
