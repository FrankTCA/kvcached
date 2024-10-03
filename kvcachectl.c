#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

enum Func {
  UNDEF,
  START,
  STOP,
  CLIENT
};

void usage_err(char *argv[]) {
  fprintf(stderr, "usage: %s <start|stop|client> <port> [[S|G|D|C] [args]]\n", argv[0]);
  exit(1);
}

void unix_err() {
  fprintf(stderr, "Could not stat information on user. OS may be unsupported.\n");
  exit(1);
}

unsigned short* get_instances(FILE *fptr) {
  unsigned short* instances;
  char buffer[BUFSIZ];
  fgets(buffer, BUFSIZ, fptr);
  char* split = strtok(buffer, " ");
  int count = 0;
  while (split != NULL) {
    instances[count] = atoi(split);
    split = strtok(NULL, " ");
    count++;
  }

  return instances;
}

int start_server(int port, unsigned short instances, FILE *fptw) {

}

int main(int argc, char **argv) {
  if (argc < 3) usage_err(argv);
  int port = atoi(argv[2]);
  enum Func func = UNDEF;
  if (strcmp(argv[1], "start")) {
    func = START;
  } else if (strcmp(argv[1], "stop")) {
    func = STOP;
  } else if (strcmp(argv[1], "client")) {
    func = CLIENT;
  } else usage_err(argv);

  struct passwd *pw = getpwuid(getuid());
  if (pw == NULL) unix_err();
  const char *homedir = pw->pw_dir;
  char *configdirstr = strdup(homedir);
  strcat(configdirstr, "/.kvcache");

  struct stat configdir = {0};
  if (stat(configdirstr, &configdir) == -1)
    mkdir(configdirstr, 0750);

  char *instancesfile = strdup(configdirstr);
  strcat(instancesfile, "/instances");

  FILE *fptr;
  fptr = fopen(instancesfile, "r");
  FILE *fptw;
  fptw = fopen(instancesfile, "w");

  if (func == START) {

  }
}
