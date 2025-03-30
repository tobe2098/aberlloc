#include <stdio.h>
#include "static_arena.h"
#include "virtual_arena.h"
int main() {
  bool a = 0;

  printf("%d\n", a);
  printf("PAGE SIZE: %d\n", _getPageSize());
  printf("Overflow: %zu\n", ((uintptr_t)UINT_MAX) * 2);
  printf("Reg: %u\n", (UINT_MAX));
  return 0;
}