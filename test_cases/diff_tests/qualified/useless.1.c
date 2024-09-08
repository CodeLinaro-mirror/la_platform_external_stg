struct foo {
};

int bar_2(struct foo* y) {
  (void) y;
  return 0;
}

int bar(const volatile struct foo* y) {
  (void) y;
  return 1;
}

int baz(int (*const volatile y)(const volatile struct foo*)) {
  (void) y;
  return 2;
}

int (*const volatile quux)(const volatile struct foo*) = &bar;
