static int my_func(int new_arg) {
  return new_arg;
}

__attribute__((__used__))
static int (*resolve_func())(int) {
  return my_func;
}

namespace ns {

// TODO: Add support for tracking type information
int func_changed(int new_arg) __attribute__((ifunc("_ZL12resolve_funcv")));

int func_added(int new_arg) __attribute__((ifunc("_ZL12resolve_funcv")));

};
