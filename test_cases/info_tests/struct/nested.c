struct nested {
  long x;
};

struct containing {
  struct nested inner;
};

struct referring {
  struct nested * inner;
};

int register_ops6(struct containing y) { (void) y; return 6; }
int register_ops7(struct containing* y) { (void) y; return 7; }
int register_ops8(struct referring y) { (void) y; return 8; }
int register_ops9(struct referring* y) { (void) y; return 9; }
