//included by build system
//#include "../src/include/gc.h"

int main() {
  void* mem = GC_malloc(8);
  GC_gcollect();
  GC_free(mem);
  
  GC_gcollect();
  GC_gcollect();

  return 0;
}
