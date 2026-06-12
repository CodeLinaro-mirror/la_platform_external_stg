static int my_func() {
  return 0;
}

__attribute__((__used__))
static int (*resolve_func())() {
  return my_func;
}

namespace ns {

int func_changed() __attribute__((ifunc("_ZL12resolve_funcv")));

int func_removed() __attribute__((ifunc("_ZL12resolve_funcv")));

};
