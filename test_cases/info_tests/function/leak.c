// Make sure we don't confuse the two foos.

struct foo;

int bar() {
  struct foo {
    int x;
  };
  struct foo j = { 0 };
  struct foo k = j;
  return j.x == k.x;
}

int baz(struct foo* j) {
  return j != 0;
}
