#define DS_IMPL
#include "ds.h"

#define NTWK_IMPL
#include "ntwk.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

typedef struct {
  int argc;
  char **argv;
  int i;
} ArgParser;

void usage_err(ArgParser a) {
  fprintf(stderr, "Usage: %s <port> <command> <paramaters>", a.argv[0]);
  fprintf(stderr, "\n");
  fprintf(stderr, "Try '%s --help' for more information.\n", a.argv[0]);
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

int connect_to_localhost(u16 port) {
  int sock = socket(PF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((short) port);

  char addrstr[NI_MAXHOST + NI_MAXSERV + 1];
  snprintf(addrstr, sizeof(addrstr), "127.0.0.1:%d", port);

  inet_pton(AF_INET, addrstr, &addr.sin_addr);

  if (connect(sock, (struct sockaddr*) &addr, sizeof(addr))) {
    perror("connect(3) error");
    exit(1);
  }

  return sock;
}

int main(int argc, char *argv[]) {
  Arena scratch = new_arena(20 * GiB);

  ArgParser argp = { .argc = argc, .argv = argv, .i = 1, };

  u8 command;
  u16 port;
  {
    s8 p = get_next_arg(&argp);
    s8 c = get_next_arg(&argp);
    if (c.len != 1) usage_err(argp);

    port = atoi(p.buf);
    command = c.buf[0];
  }

  int sock;

  switch (command) {
  case 'S': {
    s8 key = get_next_arg(&argp);

    s8 val = {0};
    if (argp.i == argp.argc) {
      val = s8_copy(&scratch, val);
      u8 ch = 0;
      while (read(STDIN_FILENO, &ch, 1)) {
        s8_modcat(&scratch, &val, (s8) { .len = 1, .buf = &ch, });
      }
    }
    else val = get_next_arg(&argp);

    no_more_args(argp);

    sock = connect_to_localhost(port);
    send_buf(sock, &command, sizeof(command));
    send_s8(sock, key);
    send_s8(sock, val);
  } break;
  case 'G': {
    s8 key = get_next_arg(&argp);
    no_more_args(argp);

    sock = connect_to_localhost(port);
    send_buf(sock, &command, sizeof(command));
    send_s8(sock, key);
  } break;
  case 'D': {
    s8 key = get_next_arg(&argp);
    no_more_args(argp);

    sock = connect_to_localhost(port);
    send_buf(sock, &command, sizeof(command));
    send_s8(sock, key);
  } break;
  case 'C': {
    no_more_args(argp);

    sock = connect_to_localhost(port);
    send_buf(sock, &command, sizeof(command));
  } break;
  default: {
    fprintf(stderr, "Error: Unrecognized command.\n");
    usage_err(argp);
  }
  }

  int status = 0;
  s8 msg = recv_s8(NULL, sock, &status);
  if (status != 0) {
    fprintf(stderr, "Error: Error when receiving server response.\n");
    exit(1);
  }
  if (msg.len < 0) {
    fprintf(stderr, "Hi\n");
    s8_fprint(stderr, msg);
  }
  else s8_print(msg);

  return 0;
}
