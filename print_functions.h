#include "ds.h"
#ifndef PRINT_FUNCTIONS_H
void print_u32_bytes(u32 a) {
    u8 b[4];
    memmove(b, &a, sizeof(b));
    printf("%c%c%c%c", b[3], b[2], b[1], b[0]);
}

void s_print_u32_bytes(char* s, u32 a) {
    u8 b[4];
    memmove(b, &a, sizeof(b));
    sprintf(s, "%s%c%c%c%c", s, b[3], b[2], b[1], b[0]);
}

void s_s8_print(char* ca, s8 s) {
    for (ssize i = 0; i < s.len; i++) {
        sprintf(ca, "%s%c", ca, s.buf[i]);
    }
}
#define PRINT_FUNCTIONS_H

#endif //PRINT_FUNCTIONS_H
