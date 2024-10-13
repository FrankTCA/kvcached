#define DS_IMPL
#include "ds.h"

#include "ntwk.h"

#include <unistd.h>

#define MB (1 << 20)
#define KEYVAL_MAX_SIZE 1 // MBs

typedef struct Store {
  struct Store *child[4];
  s8 key, val;
  bool deleted;
} Map;

Map *map_upsert(Arena *perm, Map **m, s8 key);
void map_delete(Map **m, s8 key);

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

void usage_err(char *argv[]) {
  fprintf(stderr, "Usage: %s <port>\n", argv[0]);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage_err(argv);

  Endpoint server = { .port = atoi(argv[1]), };
  if (server.port == 0) usage_err(argv);
  start_server(&server);

  Arena al = new_arena(20 * 1024 * 1024); // 20 MBs
  Arena store = new_arena(20 * 1024 * 1024); // 20 MBs

  Map *map = NULL;

  while (true) {
    Arena scratch = al;

    Endpoint c = accept_client(server);

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

  close(server.sock);
  return 0;
}

/*
 * TODO: Make errors better -- maybe send i64 instead of
 * u32, then indicate an error by a negative length?
*/
