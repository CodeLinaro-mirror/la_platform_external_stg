struct VirtualToNormal {
  virtual int print();
} virtual_to_normal;

int VirtualToNormal::print() { return 0; }

struct NormalToVirtual {
  int print();
} normal_to_virtual;

int NormalToVirtual::print() { return 1; }
