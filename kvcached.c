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
  Map *map;
} Context;

Map *map_upsert(Map **m, s8 key, bool make);
// NOTE: val must be allocated from plain malloc to be freed
// NOTE: Can only fail if key does not exist
int map_delete(Map **m, s8 key, bool free_val);

Map *map_upsert(Map **m, s8 key, bool make) {
  Map *least_deleted = NULL;

  for (u64 h = s8_hash(key); *m; h <<= 2) {
    if (s8_equals(key, (*m)->key)) return *m;

    if ((*m)->deleted && least_deleted == NULL) {
      least_deleted = *m;
    }

    m = &(*m)->child[h>>62];
  }

  if (!make) return NULL;

  if (least_deleted != NULL) *m = least_deleted;
  else *m = new(NULL, Map, 1);
  (*m)->key = s8_copy(NULL, key);
  return *m;
}

// NOTE: val must be allocated from plain malloc to be freed
// NOTE: Can only fail if key does not exist
int map_delete(Map **m, s8 key, bool free_val) {
  Map *d = map_upsert(m, key, false);
  if (d == NULL) return 1;
  free(d->key.buf);
  d->deleted = true;
  if (free_val) free(d->val.buf);
  d->key = d->val = (s8) {0};
  return 0;
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

    /* NOTE: Order of success messages depends on s8_copy not failing (it can't
     * right now) */
    Map *m = map_upsert(&ctx->map, key, true);
    if (m->val.buf != NULL) {
      free(m->val.buf);
      send_s8(sock, s8("Successfully changed value of key.\n"));
    } else send_s8(sock, s8("Successfully stored value in new key.\n"));
    m->val = s8_copy(NULL, val);

    // printf("Stored key ");
    // printf("\"");
    // s8_print(m->key);
    // printf("\"");
    // printf(" with value ");
    // printf("\"");
    // s8_print(m->val);
    // printf("\"");
    // printf("\n");
    printf("Stored key.\n");
  } break;
  case 'G': {
    s8 key = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (key.buf == NULL) break;

    Map *m = map_upsert(&ctx->map, key, false);
    if (m == NULL) {
      send_err(sock, s8("Error: No such key.\n"));
      printf("Sent error.\n");
      break;
    }

    send_s8(sock, m->val);
    printf("Sent key.\n");
  } break;
  case 'D': {
    s8 key = recv_s8(&ctx->scratch, sock, &status);
    if (status) break;
    if (key.buf == NULL) break;

    if (map_delete(&ctx->map, key, true)) {
      send_err(sock, s8("Error: No such key.\n"));
      printf("Sent error.\n");
    } else {
      send_s8(sock, s8("Successfully deleted key.\n"));
      printf("Deleted key.\n");
    }
  } break;
  case 'C': {
    // TODO: Traverse tree to free memory
    ctx->map = NULL;
    send_s8(sock, s8("Successfully cleared cache.\n"));
    printf("Cleared cache.\n");
  } break;
  default: {
    fprintf(stderr, "Warning: Client sent unrecognized request %d.\n", command);
    send_err(sock, s8("Error: Unrecognized or invalid request.\n"));
  }
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
    .scratch = new_arena(20 * MB),
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
