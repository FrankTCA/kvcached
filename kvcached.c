#include <stdio.h>
#include <errno.h>

#define NTWK_IMPL
#include "ntwk.h"

typedef struct Map {
  struct Map *child[4];
  s8 key, val;
  bool deleted;
} Map;

typedef struct {
  Arena scratch;
  Arena store;
  Map *map;
} Context;

Map *map_upsert(Arena *perm, Map **m, s8 key);
void map_delete(Map **m, s8 key);

Map *map_upsert(Arena *perm, Map **m, s8 key) {
  Map *least_deleted = NULL;

  for (u64 h = s8_hash(key); *m; h <<= 2) {
    if (s8_equals(key, (*m)->key)) return *m;

    if ((*m)->deleted && least_deleted == NULL) {
      least_deleted = *m;
    }

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

// struct ev_io *on_connect(i32 sock, Server *s) {
//   return malloc(sizeof(struct ev_io));
// }

// void on_disconnect(i32 sock, Server *s, struct ev_io *cio) {
//   free(cio);
// }

int on_recv(i32 sock, Server *s) {
  Context *ctx = s->data;

  int status = 0;

  u8 command = recv_u8(sock, &status);
  if (status) return status;

  switch (command) {
  case 'S': {
    s8 key = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (key.buf == NULL) break;

    s8 val = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (val.buf == NULL) break;

    Map *m = map_upsert(&ctx->store, &ctx->map, key);
    if (m->val.buf == NULL) m->val = s8_copy(&ctx->store, val);

    // printf("Stored key ");
    // printf("\"");
    // s8_print(m->key);
    // printf("\"");
    // printf(" with value ");
    // printf("\"");
    // s8_print(m->val);
    // printf("\"");
    // printf("\n");
    printf("Stored key\n");
  } break;
  case 'G': {
    s8 key = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (key.buf == NULL) break;

    Map *m = map_upsert(NULL, &ctx->map, key);
    if (m == NULL) {
      send_msg(sock, s8("Error: No such key\n"));
      printf("Sent error\n");
      break;
    }

    send_s8(sock, m->val);
    printf("Sent key\n");
  } break;
  case 'D': {
    s8 key = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (key.buf == NULL) break;

    map_delete(&ctx->map, key);
    printf("Deleted key\n");
  } break;
  case 'C': {
    // TODO: Traverse tree to free memory
    ctx->map = NULL;
    printf("Cleared cache\n");
  } break;
  default:
    fprintf(stderr, "Warning: Client send unrecognized request %d\n", command);
  }

  return status;
}

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s <port>\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage_err(argv);

  Context ctx = {
    .scratch = new_arena(20 * 1024 * 1024), // 20 MiBs
    .store = new_arena(20 * 1024 * 1024), // 20 MiBs
  };

  Server s = {
    .port = atoi(argv[1]),
    .on_recv = on_recv,
    .data = &ctx,
    // .on_connect = on_connect,
    // .on_disconnect = on_disconnect,
  };
  if (s.port == 0) usage_err(argv);

  start_server(s);

  return 0;
}
