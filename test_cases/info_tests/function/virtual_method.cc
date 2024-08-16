struct Foo {
  virtual int bar();
  virtual int baz();
} foo;

int Foo::bar() { return 0; }
int Foo::baz() { return 1; }
