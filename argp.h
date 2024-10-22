// No IMPL or anything because this is tiny.

#ifndef ARGP_H
#define ARGP_H

#define DS_IMPL
#include "ds.h"

typedef struct ArgParser ArgParser;
struct ArgParser {
  int argc;
  char **argv;
  int i;
  s8 extra_usage;
  void (*usage_err)(ArgParser);
};

s8 get_next_arg(ArgParser *a) {
  if (a->i >= a->argc) a->usage_err(*a);

  s8 ret = { .buf = a->argv[a->i], };
  ret.len = strlen(ret.buf);

  a->i += 1;

  return ret;
}

void no_more_args(ArgParser a) {
  if (a.argc > a.i) a.usage_err(a);
}

#endif // ARGP_H
