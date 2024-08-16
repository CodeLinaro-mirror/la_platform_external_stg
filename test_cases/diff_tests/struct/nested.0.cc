struct nested {
  int x;
};

struct containing {
  struct nested inner;
};

struct referring {
  struct nested * inner;
};

int register_ops6(containing) { return 6; }
int register_ops7(containing*) { return 7; }
int register_ops8(referring) { return 8; }
int register_ops9(referring*) { return 9; }
