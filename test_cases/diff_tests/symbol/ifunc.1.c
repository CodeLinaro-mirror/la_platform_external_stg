static int my_func(int new_arg) {
  return new_arg;
}

__attribute__((__used__))
static int (*resolve_func(void))(int) {
  return my_func;
}

// TODO: Add support for tracking type information
int func_changed(int new_arg) __attribute__((ifunc("resolve_func")));

int func_added(int new_arg) __attribute__((ifunc("resolve_func")));
