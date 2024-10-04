#define DS_IMPL
#include "ds.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define MB (1 << 20)
#define KEYVAL_MAX_SIZE 1 // MBs

typedef struct {
  int sock;
  struct sockaddr_storage addr;
  socklen_t addr_len;
} Client;

typedef struct Store {
  struct Store *child[4];
  s8 key, val;
  bool deleted;
} Map;

Map *map_upsert(Arena *perm, Map **m, s8 key);
void map_delete(Map **m, s8 key);

ssize_t recv_buf(Client c, u8 *buf, ssize_t buf_len);
u32 recv_u32(Client c);
u8 recv_u8(Client c);
s8 recv_s8(Arena *perm, Client c);
void send_u32(Client c, u32 a);
void send_msg(Client c, s8 s);
void send_s8(Client c, s8 s);

Map *map_upsert(Arena *perm, Map **m, s8 key) {
  Map *least_deleted = NULL;

  for (u64 h = s8_hash(key); *m; h <<= 2) {
    if (s8_equals(key, (*m)->key)) return *m;

    if ((*m)->deleted && least_deleted == NULL) {
      least_deleted = *m;
    }

    printf("%ld\n", h>>62);

    m = &(*m)->child[h>>62];
  }

  if (!perm) return NULL;
  if (least_deleted != NULL) *m = least_deleted;
  else *m = new(perm, Map, 1);
  (*m)->key = s8_copy(perm, key);
  return *m;
}

// TODO: Also clear values to reduce memory usage
void map_delete(Map **m, s8 key) {
  Map *d = map_upsert(NULL, m, key);
  if (d == NULL) return;
  d->deleted = true;
  d->key = d->val = (s8) {0};
}

ssize_t recv_buf(Client c, u8 *buf, ssize_t buf_len) {
  ssize_t recvd = 0;

  while (recvd < buf_len) {
    ssize_t bytes = recv(c.sock, buf + recvd, buf_len - recvd, 0);

    if (bytes < 0) {
      perror("recv(3) error");
      exit(1);
    }

    recvd += bytes;
  }

  return recvd;
}

u32 recv_u32(Client c) {
  u32 ret = 0;
  recv_buf(c, (u8 *)(&ret), 4);
  return ntohl(ret);
}

u8 recv_u8(Client c) {
  u8 ret = 0;
  recv_buf(c, &ret, 1);
  return ret;
}

s8 recv_s8(Arena *perm, Client c) {
  s8 s = {0};
  s.len = recv_u32(c);
  if (s.len > KEYVAL_MAX_SIZE * MB) {
    fprintf(stderr, "Error: Packet too large. Ignoring connection.\n");
    send_s8(c, s8("Error: Key/Value cannot be larger than "
                  strify(KEYVAL_MAX_SIZE)
                  " MiBs\n"));
    close(c.sock);
    return (s8) {0};
  }

  s.buf = new(perm, u8, s.len);
  recv_buf(c, s.buf, s.len);
  return s;
}

void send_u32(Client c, u32 a) {
  u8 b[4];
  memmove(b, &a, sizeof(b));
  send(c.sock, b, sizeof(b), 0);
}

void send_msg(Client c, s8 s) {
  send(c.sock, s.buf, s.len * sizeof(u8), 0);
}

void send_s8(Client c, s8 s) {
  send_u32(c, s.len);
  send(c.sock, s.buf, s.len * sizeof(u8), 0);
}

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s <port>\n", argv[0]);

  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage_err(argv);

  u16 port = atoi(argv[1]);
  if (port == 0) usage_err(argv);

  const int fd = socket(PF_INET, SOCK_STREAM, 0);
  {
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = ntohs(port),
  };

  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr))) {
    perror("bind(3) error");
    exit(1);
  }

  socklen_t addr_len = sizeof(addr);
  getsockname(fd, (struct sockaddr *) &addr, &addr_len);
  printf("Server is on port %d\n", (int) ntohs(addr.sin_port));

  if (listen(fd, 1)) {
    perror("listen(3) error");
    exit(1);
  }

  Arena al = new_arena(20 * 1024 * 1024); // 20 MBs
  Arena store = new_arena(20 * 1024 * 1024); // 20 MBs

  Map *map = NULL;

  while (true) {
    Arena scratch = al;

    Client c = {0};
    c.addr_len = sizeof(c.addr);
    c.sock = accept(fd, (struct sockaddr *) &c.addr, &c.addr_len);

    u8 command = recv_u8(c);
    switch (command) {
    case 'S': {
      // send_msg(c, s8("SENDKEY\n"));
      s8 key = recv_s8(&scratch, c);
      if (key.buf == NULL) continue;

      // send_msg(c, s8("SENDVAL\n"));
      s8 val = recv_s8(&scratch, c);
      if (val.buf == NULL) continue;

      Map *m = map_upsert(&store, &map, key);
      if (m->val.buf == NULL) m->val = s8_copy(&store, val);

      // s8_print(key);
      // printf("\n");
      // s8_print(val);
      // printf("\n");
      printf("Stored key\n");
    } break;
    case 'G': {
      // send_msg(c, s8("SENDKEY\n"));
      s8 key = recv_s8(&scratch, c);
      if (key.buf == NULL) continue;

      Map *m = map_upsert(NULL, &map, key);
      if (m == NULL) send_s8(c, (s8) {0});
      else send_s8(c, m->val);

      printf("Sent key\n"); // TODO: Make diagnostics better
    } break;
    case 'D': {
      // send_msg(c, s8("SENDKEY\n"));
      s8 key = recv_s8(&scratch, c);
      if (key.buf == NULL) continue;

      map_delete(&map, key);

      printf("Deleted key\n");
    } break;
    case 'C': {
      map = NULL;
      free(store.buf);
      store = new_arena(20 * 1024 * 1024); // TODO: Centralize this
      printf("Cleared cache\n");
    } break;
    default: fprintf(stderr, "Warning: Client sent unrecognized request!\n%d\n", command);
    }

    close(c.sock);
  }

  close(fd);
  return 0;
}

/*
 * TODO: Make errors better -- maybe send i64 instead of
 * u32, then indicate an error by a negative length?
*/
