// Test that types in Linux kernel binaries are overridden by types output by
// the __GENDWARFKSYMS_EXPORT macro. That macro is defined as follows:
//
// #define __GENDWARFKSYMS_EXPORT(sym)
//    static typeof(sym) *__gendwarfksyms_ptr_##sym __used
//    __section(".discard.gendwarfksyms") = &sym;
//
// That is, it creates a discarded variable of type pointer to the original
// exported symbol's type, with a prefix of "__gendwarfksyms_ptr_" in its name.
//
// This test checks that we are able to override variables to have an
// arbitrary exported type.

// Create a symbol of type int, a dummy ksymtab entry and the DWARF override.
#define GENDWARFKSYMS_EXPORT(type, sym)                        \
  int sym = __LINE__;                                          \
  int __ksymtab_##sym = __LINE__;                              \
  static type* __gendwarfksyms_ptr_##sym __attribute__((used))

struct my_struct {
  int foo;
};

// struct my_struct my_symbol;
GENDWARFKSYMS_EXPORT(struct my_struct, my_symbol);

// void* voidptr_symbol;
GENDWARFKSYMS_EXPORT(void*, voidptr_symbol);

// Ensure IsLinuxKernelBinary() evaluates to true.
char my_ksymtab_strings[0] __attribute__((section("__ksymtab_strings")));
