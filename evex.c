#include <stdio.h>

#define NTWK_IMPL
#include "ntwk.h"

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s <port>\n", argv[0]);
  exit(1);
}

void on_recv(struct ev_loop *loop, struct ev_io *io, int revents) {
  if (EV_ERROR & revents) {
    perror("Got invalid libev event");
    return;
  }

  u8 buf[1024] = {0};
  int nread = recv(io->fd, buf, arrlen(buf), 0);

  if (nread == 0) {
    ev_io_stop(loop, io);
    free(io);
    return;
  }

  send_s8(io->fd, s8("Server: Yo what's up!\n"));
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage_err(argv);

  Server server = { .port = atoi(argv[1]), .on_recv = on_recv, };
  if (server.port == 0) usage_err(argv);
  start_server(server);
}
