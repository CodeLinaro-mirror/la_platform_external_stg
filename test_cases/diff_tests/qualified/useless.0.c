struct foo {
};

int bar(struct foo y) {
  (void) y;
  return 0;
}

int bar_1(const volatile struct foo* y) {
  (void) y;
  return 1;
}

int baz(int (*y)(struct foo)) {
  (void) y;
  return 2;
}

int (*quux)(struct foo) = &bar;
