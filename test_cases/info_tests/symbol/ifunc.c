static void my_func(void) {}

__attribute__((__used__))
static void (*resolve_func(void))(void) {
  return my_func;
}

void func(void) __attribute__((ifunc("resolve_func")));
