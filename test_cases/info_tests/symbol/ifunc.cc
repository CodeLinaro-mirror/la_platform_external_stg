static void my_func() {}

__attribute__((__used__))
static void (*resolve_func())() {
  return my_func;
}

namespace ns {

void func() __attribute__((ifunc("_ZL12resolve_funcv")));

}
