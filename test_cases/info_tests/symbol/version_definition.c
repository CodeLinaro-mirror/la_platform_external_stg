// Test for versioned symbol
//
// Currently version information is unsupported by ELF reader, so tests may
// produce wrong results.
// TODO: remove statement above after support is implemented

int versioned_foo(void) { return 1; }

__asm__(".symver versioned_foo_v1, versioned_foo@@VERS_1");
int versioned_foo_v1(void) { return 2; }

__asm__(".symver versioned_foo_v2, versioned_foo@VERS_2");
int versioned_foo_v2(void) { return 3; }

__asm__(".symver versioned_foo_v3, versioned_foo@VERS_3");
int versioned_foo_v3(void) { return 4; }

// Using a libc function helps to add the "version needs" section
// in addition to the "version definitions". This helps to catch
// bugs when a file has both of these sections.
int printf(const char *format, ...);

int test() {
  printf("test");
  return 0;
}
