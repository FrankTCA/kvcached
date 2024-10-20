#ifndef NTWK_H
#define NTWK_H

#define DS_IMPL
#include "ds.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#ifndef RECV_MAX_SIZE
#define RECV_MAX_SIZE (1 * GiB) // bytes
#endif // RECV_MAX_SIZE

#include <ev.h>

typedef struct Server Server;
typedef struct Server {
  struct ev_io io;
  int (*on_recv)(i32 sock, Server *s);
  struct ev_io *(*on_connect)(i32 sock, Server *s);
  void (*on_disconnect)(i32 sock, Server *s, struct ev_io *cio);
  u16 port;
  int num_clients;
  void *data;
} Server;

// Status is sticky -- it only changes upon non-continue
ssize_t recv_buf(i32 sock, u8 *buf, ssize_t buf_len, int *status);
i64 recv_i64(i32 sock, int *status);
u8 recv_u8(i32 sock, int *status);
s8 recv_s8(Arena *perm, i32 sock, int *status);

int send_buf(i32 sock, void *buf, ssize n);
int send_i64(i32 sock, i64 a);
int send_err(i32 sock, s8 str);
int send_s8(i32 sock, s8 str);

void on_recv_wrap(struct ev_loop *loop, struct ev_io *io, int revents);
void on_connect_wrap(struct ev_loop *loop, struct ev_io *io, int revents);
void on_accept(struct ev_loop *loop, struct ev_io *io, int revents);
void start_server(Server s);

#endif // NTWK_H

#ifdef NTWK_IMPL
#ifndef NTWK_IMPL_GUARD
#define NTWK_IMPL_GUARD

ssize_t recv_buf(i32 sock, u8 *buf, ssize_t buf_len, int *status) {
  ssize_t recvd = 0;

  while (recvd < buf_len) {
    ssize_t bytes = recv(sock, buf + recvd, buf_len - recvd, 0);

    if (bytes < 0) {
      perror("recv(3) error");
      *status = -1;
      return recvd;
    }

    if (bytes == 0) {
      *status = 1;
      return recvd;
    }

    recvd += bytes;
  }

  return recvd;
}

i64 recv_i64(i32 sock, int *status) {
  u8 buf[8] = {0};
  recv_buf(sock, buf, arrlen(buf), status);
  return ((i64)buf[7] <<  0) | ((i64)buf[6] <<  8) |
         ((i64)buf[5] << 16) | ((i64)buf[4] << 24) |
         ((i64)buf[3] << 32) | ((i64)buf[2] << 40) |
         ((i64)buf[1] << 48) | ((i64)buf[0] << 56);
}

u8 recv_u8(i32 sock, int *status) {
  u8 ret = 0;
  recv_buf(sock, &ret, 1, status);
  return ret;
}

s8 recv_s8(Arena *perm, i32 sock, int *status) {
  s8 ret = {0};
  ret.len = recv_i64(sock, status);
  if (*status) return ret; // TODO: Should this indicate that it failed at this location?

  if (ret.len < 0) ret.len *= -1;

  if (ret.len > RECV_MAX_SIZE) {
    fprintf(stderr, "Error: Packet too large (%ld bytes). "
                    "Ignoring connection.\n", ret.len);
    send_s8(sock, s8("Error: Packet cannot be larger than "
                     strify(RECV_MAX_SIZE) " bytes.\n"));
    close(sock);
    *status = -1;
    return ret;
  }

  ret.buf = new(perm, u8, ret.len);
  recv_buf(sock, ret.buf, ret.len, status);
  return ret;
}

int send_buf(i32 sock, void *buf, ssize n) {
  int status = send(sock, buf, n, 0);
  if (status < 0) {
    perror("send(3) error");
    return status;
  }
  return 0;
}

int send_i64(i32 sock, i64 a) {
  u8 buf[8];
  buf[7] = (a & 0x00000000000000ff);
  buf[6] = (a & 0x000000000000ff00) >> 8;
  buf[5] = (a & 0x0000000000ff0000) >> 16;
  buf[4] = (a & 0x00000000ff000000) >> 24;
  buf[3] = (a & 0x000000ff00000000) >> 32;
  buf[2] = (a & 0x0000ff0000000000) >> 40;
  buf[1] = (a & 0x00ff000000000000) >> 48;
  buf[0] = (a & 0xff00000000000000) >> 56;
  return send_buf(sock, buf, sizeof(buf));
}

int send_err(i32 sock, s8 str) {
  int status = send_i64(sock, -str.len);
  if (status) return status;

  return send_buf(sock, str.buf, str.len * sizeof(u8));
}

int send_s8(i32 sock, s8 str) {
  int status = send_i64(sock, str.len);
  if (status) return status;
  return send_buf(sock, str.buf, str.len * sizeof(u8));
}

void on_recv_wrap(struct ev_loop *loop, struct ev_io *io, int revents) {
  if (EV_ERROR & revents) {
    perror("Invalid libev event");
    return;
  }

  Server *s = io->data;
  if (s->on_recv(io->fd, s)) {
    ev_io_stop(loop, io);
    s->num_clients -= 1;
    if (s->on_disconnect) s->on_disconnect(io->fd, s, io);
    else free(io);
    return;
  }
}

void on_connect_wrap(struct ev_loop *loop, struct ev_io *io, int revents) {
  if (EV_ERROR & revents) {
    perror("Invalid libev event");
    return;
  }

  struct sockaddr_in caddr = {0};
  socklen_t clen = sizeof(caddr);
  i32 csock = accept(io->fd, (struct sockaddr *) &caddr, &clen);
  if (csock < 0) {
    perror("accept(3) error");
    return;
  }

  Server *s = (Server *) io;
  s->num_clients += 1;

  struct ev_io *w_client = s->on_connect ?
                           s->on_connect(csock, s) :
                           malloc(sizeof(struct ev_io));

  w_client->data = s;

  ev_io_init(w_client, on_recv_wrap, csock, EV_READ);
  ev_io_start(loop, w_client);
}

void start_server(Server s) {
  i32 sock = socket(PF_INET, SOCK_STREAM, 0);
  {
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = ntohs(s.port),
  };

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    perror("bind(3) error");
    exit(1);
  }

  socklen_t addr_len = sizeof(addr);
  getsockname(sock, (struct sockaddr *) &addr, &addr_len);
  printf("Server is on port %d.\n", (int) ntohs(addr.sin_port));

  if (listen(sock, 1)) {
    perror("listen(3) error");
    exit(1);
  }

  struct ev_loop *loop = ev_default_loop(0);
  ev_io_init(&s.io, on_connect_wrap, sock, EV_READ);
  ev_io_start(loop, &s.io);

  while (1) {
    ev_loop(loop, 0);
  }
}

// Client accept_client(Client s) {
//   Endpoint ret = {0};
//   struct sockaddr_in addr = {0};
//   socklen_t addr_len = sizeof(addr);
//   ret.sock = accept(s.sock, (struct sockaddr *) &addr, &addr_len);
//   return ret;
// }

#endif // DS_IMPL_GUARD
#endif // DS_IMPL
