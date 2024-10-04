#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <math.h>

#include "client.h"

#define DEFAULT_TRACKING_PORT 32782

enum Protocol_Method {
  UNDEFINED,
  SAVE,
  GET,
  DELETE,
  CLEAR
};

void usage_err(char *argv[]) {
  fprintf(stderr, "usage: %s <start|stop|client> <port> [[S|G|D|C] [args]]\n", argv[0]);
  exit(1);
}

void unix_err() {
  fprintf(stderr, "Could not stat information on user. OS may be unsupported.\n");
  exit(1);
}

void val_error() {
  fprintf(stderr, "Could not retrieve value needed!\n");
  exit(1);
}

void server_not_running_err(int port) {
  fprintf(stderr, "There is no server at port %i!\n", port);
  exit(1);
}

/*char* get_instances_content(FILE *fptr) {
  char buffer[BUFSIZ];
  fgets(buffer, BUFSIZ, fptr);
  return strdup(buffer);
}*/

/*unsigned short* get_instances(FILE *fptr) {
  unsigned short* instances;
  char* instances_str = get_instances_content(fptr);
  char* split = strtok(instances_str, " ");
  int count = 0;
  while (split != NULL) {
    instances[count] = atoi(split);
    split = strtok(NULL, " ");
    count++;
  }

  return instances;
}

void write_instances(FILE *fptw, char* instances, char* new_instance) {
  char buffer[BUFSIZ];
  strncpy(instances, " ", 1);
  strncpy(instances, new_instance, strlen(new_instance));
  fprintf(fptw, "%s", instances);
}*/

void start_server(const char* port) {
  pid_t pid = fork();
  int status = 0;
  int the_pid = 0;
  if (pid == 0) {
    status = execl("/usr/local/bin/kvcached", "/usr/local/bin/kvcached", strdup(port), NULL);
    if (status != 0) {
      fprintf(stderr, "kvcached exec failed: %d\n", status);
    }
    exit(1);
  } else {
    sleep(3);
    the_pid = pid;
    printf("kvcached server started on PID %i\n", the_pid);
    char pid_str[7];
    sprintf(pid_str, "%i", the_pid);
    set_value(DEFAULT_TRACKING_PORT, strdup(port), pid_str);
  }
}

int stop_server(const char* port) {
  char* pid_ch = get_value(DEFAULT_TRACKING_PORT, strdup(port));
  if (pid_ch == NULL) val_error();

  pid_t the_pid = atoi(pid_ch);
  kill(the_pid, SIGKILL);
  return 0;
}

void run_client(int port, enum Protocol_Method method, int argc, char** argv) {
  if (!test_running(port)) server_not_running_err(port);
  if (method == SAVE) {
    if (argc < 5) usage_err(argv);
    char* value = "";
    for (int i = 4; i < argc; i++) {
      strncat(value, argv[i], strlen(argv[i]));
      strncat(value, " ", 1);
    }
    set_value(port, argv[3], value);
    printf("Value set!\n");
    exit(0);
  } else if (method == GET) {
    if (argc != 4) usage_err(argv);
    char* value = get_value(port, argv[3]);
    if (value == NULL) val_error();
    printf("%s\n", value);
    exit(0);
  } else if (method == DELETE) {
    if (argc != 4) usage_err(argv);
    delete(port, argv[3]);
    printf("Value deleted!\n");
    exit(0);
  } else if (method == CLEAR) {
    if (argc != 3) usage_err(argv);
    clear(port);
    printf("Memory cleared!\n");
    exit(0);
  }
}

void parse_client(int port, int argc, char** argv) {
  if (argc < 3) usage_err(argv);

  enum Protocol_Method method = UNDEFINED;
  if (strcmp(argv[3], "S")) {
    method = SAVE;
  } else if (strcmp(argv[3], "G")) {
    method = GET;
  } else if (strcmp(argv[3], "D")) {
    method = DELETE;
  } else if (strcmp(argv[3], "C")) {
    method = CLEAR;
  }

  run_client(port, method, argc, argv);
}

int main(int argc, char **argv) {
  if (argc < 3) usage_err(argv);
  const char* port = argv[2];

  char default_port[6];
  sprintf(default_port, "%d", DEFAULT_TRACKING_PORT);
  if (!test_running(DEFAULT_TRACKING_PORT)) {
    start_server(default_port);
  }
  printf("Before func switch.\n");
  if (strcmp(argv[1], "start")==0) {
    printf("Func switch start.\n");
    start_server(port);
  }
  if (strcmp(argv[1], "stop")==0) {
    stop_server(port);
  }
  if (strcmp(argv[1], "client")==0) {
    parse_client(atoi(port), argc, argv);
  }
  return 0;
}
