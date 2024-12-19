#include <stdio.h>
#include "static_arena.h"
#include "utils.h"

int main() {
  bool a = 0;

  printf("%d\n", a);
  printf("PAGE SIZE: %d\n", _getPageSize());

  return 0;
}