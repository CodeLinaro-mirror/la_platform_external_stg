struct Func {
  // change
  long change_return_type();

  // add or remove
  int add_parameter(int add);
  int remove_parameter();
  int change_parameter_type(long parameter);
  int rename_new();

  // no diff
  int change_parameter_name(int add);

  long x;
} var;

long Func::change_return_type() { return 0; }
int Func::add_parameter(int add) { return add; }
int Func::remove_parameter() { return 0; }
int Func::change_parameter_type(long parameter) { return parameter; }
int Func::rename_new() { return 0; }
int Func::change_parameter_name(int add) { return add; }
