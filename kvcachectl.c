#include <stdio.h>

#define ARGP_IMPL
#include "argp.h"

void usage_err(ArgParser a) {
  fprintf(stderr, "Usage: %s ", a.argv[0]);
  s8_fprint(stderr, a.extra_usage);
  fprintf(stderr, "\n");
  fprintf(stderr, "Try '%s --help' for more information.\n", a.argv[0]);
  exit(1);
}

int main() {
  ArgParser argp = {
    .argc = argc, .argv = argv,
    .i = 1,
    .extra_usage = s8("<command> <name>"),
    .usage_err = usage_err, 
  };

  s8 command = get_next_arg(&argp);
  s8 name = get_next_arg;

  if (s8_equals(command, s8("start"))) {
    int pid = fork();
    if (pid == 0) printf("I was just born\n");
    else printf("I just gave birth\n");
  } else if (s8_equals(command, s8("stop"))) {

  } else {
  }

  return 0;
}
