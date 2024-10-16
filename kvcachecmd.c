#define DS_IMPL
#include "ds.h"
#include "print_functions.h"

typedef struct {
  int argc;
  char **argv;
  int i;
  s8 extra_usage;
} ArgParser;

void usage_err(ArgParser a) {
  fprintf(stderr, "Usage: %s ", a.argv[0]);
  s8_print(a.extra_usage);
  printf("\n");
  exit(1);
}

s8 get_next_arg(ArgParser *a) {
  if (a->i >= a->argc) usage_err(*a);

  s8 ret = { .buf = a->argv[a->i], };
  ret.len = strlen(ret.buf);

  a->i += 1;

  return ret;
}

void no_more_args(ArgParser a) {
  if (a.argc > a.i) usage_err(a);
}

int main(int argc, char *argv[]) {
  ArgParser argp = { .argc = argc, .argv = argv, .i = 1, };

  u8 command;
  {
    argp.extra_usage = s8("'S|G|D|C' ...");
    // s8 p = get_next_arg(&argp);
    s8 c = get_next_arg(&argp);
    if (c.len != 1) usage_err(argp);

    // port = atoi(p.buf);
    command = c.buf[0];
  }

  switch (command) {
  case 'S': {
    argp.extra_usage = s8("S <key> <val>");
    s8 key = get_next_arg(&argp);
    s8 val = get_next_arg(&argp);
    no_more_args(argp);

    printf("%c", command);
    print_i64_bytes(key.len);
    s8_print(key);
    print_i64_bytes(val.len);
    s8_print(val);
  } break;
  case 'G': {
    argp.extra_usage = s8("G <key>");
    s8 key = get_next_arg(&argp);
    no_more_args(argp);

    printf("%c", command);
    print_i64_bytes(key.len);
    s8_print(key);
  } break;
  case 'D': {
    argp.extra_usage = s8("D <key>");
    s8 key = get_next_arg(&argp);
    no_more_args(argp);

    printf("%c", command);
    print_i64_bytes(key.len);
    s8_print(key);
  } break;
  case 'C': {
    argp.extra_usage = s8("C");
    no_more_args(argp);

    printf("%c", command);
  } break;
  default: assert(!"Unimplemented or unrecognized command");
  }

  return 0;
}
