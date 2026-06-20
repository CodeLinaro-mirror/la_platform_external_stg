static int my_func(void) {
  return 0;
}

__attribute__((__used__))
static int (*resolve_func(void))(void) {
  return my_func;
}

int func_changed(void) __attribute__((ifunc("resolve_func")));

int func_removed(void) __attribute__((ifunc("resolve_func")));
