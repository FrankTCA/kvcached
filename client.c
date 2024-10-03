#define DS_IMPL
#include "ds.h"

// Does byte order matter?
void print_u32_bytes(u32 a) {
  u8 b[4];
  memmove(b, &a, sizeof(b));
  printf("%c%c%c%c", b[3], b[2], b[1], b[0]);
}

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s 'S|G|D|C' <key> <val>\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc < 2) usage_err(argv);
  if (strlen(argv[1]) != 1) usage_err(argv);

  u8 command = argv[1][0];

  switch (command) {
  case 'S': {
    if (argc < 4) usage_err(argv);

    s8 key = { .buf = (u8 *) argv[2], .len = strlen(argv[2]), };
    s8 val = { .buf = (u8 *) argv[3], .len = strlen(argv[3]), };

    printf("%c", command);
    print_u32_bytes(key.len);
    s8_print(key);
    print_u32_bytes(val.len);
    s8_print(val);
  } break;
  case 'G': {
    if (argc != 3) usage_err(argv);

    s8 key = { .buf = (u8 *) argv[2], .len = strlen(argv[2]), };

    printf("%c", command);
    print_u32_bytes(key.len);
    s8_print(key);
  } break;
  case 'D': {
    if (argc != 3) usage_err(argv);

    s8 key = { .buf = (u8 *) argv[2], .len = strlen(argv[2]), };

    printf("%c", command);
    print_u32_bytes(key.len);
    s8_print(key);
  } break;
  case 'C': {
    if (argc != 2) usage_err(argv);
    printf("%c", command);
  } break;
  default: assert(!"Unimplemented or unrecognized command");
  }

  return 0;
}
