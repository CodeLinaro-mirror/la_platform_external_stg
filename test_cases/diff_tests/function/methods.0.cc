struct Func {
  // change
  int change_return_type();

  // add or remove
  int add_parameter();
  int remove_parameter(int remove);
  int change_parameter_type(int parameter);
  int rename_old();

  // no diff
  int change_parameter_name(int remove);

  int x;
} var;

int Func::change_return_type() { return 0; }
int Func::add_parameter() { return 0; }
int Func::remove_parameter(int remove) { return remove; }
int Func::change_parameter_type(int parameter) { return parameter; }
int Func::rename_old() { return 0; }
int Func::change_parameter_name(int remove) { return remove; }
