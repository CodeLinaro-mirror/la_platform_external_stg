struct Func {
  // change
  long change_return_type();

  // add or remove
  int add_parameter(int);
  int remove_parameter();
  int change_parameter_type(long);
  int rename_new();

  // no diff
  int change_parameter_name(int);

  long x;
} var;

long Func::change_return_type() { return 0; }
int Func::add_parameter(int) { return 0; }
int Func::remove_parameter() { return 0; }
int Func::change_parameter_type(long) { return 0; }
int Func::rename_new() { return 0; }
int Func::change_parameter_name(int) { return 0; }
