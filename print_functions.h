#include "ds.h"
#ifndef PRINT_FUNCTIONS_H

void print_i64_bytes(i64 a) {
  u8 b[8];
  memmove(b, &a, sizeof(b));
  printf("%c%c%c%c%c%c%c%c", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
}

#define PRINT_FUNCTIONS_H

#endif //PRINT_FUNCTIONS_H
