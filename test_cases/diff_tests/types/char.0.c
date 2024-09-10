// tweaked due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112372
int u(char c) { (void)c; return 0; }
int v(unsigned char c) { (void)c; return 1; }
int w(signed char c) { (void)c; return 2; }
int x(char c) { (void)c; return 3; }
int y(unsigned char c) { (void)c; return 4; }
int z(signed char c) { (void)c; return 5; }
