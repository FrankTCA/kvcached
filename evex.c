#include <stdio.h>

#define NTWK_IMPL
#include "ntwk.h"

int on_recv(i32 sock, void *data) {
  u8 buf[1024] = {0};
  int nread = recv(sock, buf, arrlen(buf), 0);
  if (nread == 0) return 1;

  printf("%d\n", nread);
  ctx->msg_len = nread;

  return 0;
}

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s <port>\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage_err(argv);

  Server server = {
    .port = atoi(argv[1]),
    .on_recv = on_recv,
  };
  if (server.port == 0) usage_err(argv);

  start_server(server);
}
