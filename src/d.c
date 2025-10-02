#define err(...) do { \
    fprintf(stderr, "Error: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    abort(); \
} while (0);

#define warning(...) do { \
    fprintf(stderr, "Warning: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
} while (0);

#define NTWK_IMPL
#include "ntwk.h"

#define DS_IMPL
#include "ds.h"

typedef struct Cache Cache;
typedef struct Cache {
    Cache *child[4];
    s8 key;
    s8 val;
} Cache;

typedef struct {
    Cache **cache;
} ConnectionData;

s8 *cache_upsert(Arena *perm, Cache **m, s8 key, bool make) {
    for (u64 h = s8_hash(key); *m; h <<= 2) {
        if (s8_equals(key, (*m)->key)) return &(*m)->val;
        m = &(*m)->child[h>>62];
    }
    if (!make) return 0;
    *m = new(perm, Cache, 1);
    (*m)->key = key;
    return &(*m)->val;
}

void connection_main() {
    Connection *c = (Connection *) aco_get_arg();
    ConnectionData *data = (ConnectionData *) c->data;
    Arena key_arena = new_arena(20 * GiB),
          val_arena = { .cap = PTRDIFF_MAX, }; // non-local

    while (!c->status) {
        printf("Socket: %d %p\n", c->sock, aco_get_co());
        u64 cmd = recv_i64(&c);
        if (c->status) goto end;

        switch (cmd) {
        case 'S': {
            u64 timeout_seconds = recv_i64(&c);
            s8 key = recv_s8(&key_arena, &c),
               val = recv_s8(&val_arena, &c);
            if (c->status) goto end;

            *cache_upsert(NULL, data->cache, key, 1) = val;
        } break;
        case 'G': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;

            s8 *val = cache_upsert(NULL, data->cache, key, 0);
            if (c->status) goto end;
            if (val) s8_print(*val);
            else printf("Value does not exist!");
            printf("\n");
        } break;
        case 'E': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;
        } break;
        case 'D': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;
        } break;
        case 'C': {
        } break;
        case 'T': {
            u64 timeout_seconds = recv_i64(&c);
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;
        } break;
        }

        key_arena.len = val_arena.len = 0;
    }

end:
    free(key_arena.buf);
    key_arena = (Arena) {0};
    val_arena = (Arena) {0};
    aco_exit();
}

int main() {
    aco_thread_init(NULL);

    Server server = {
        .main_co = aco_create(0, 0, 0, 0, 0),
        .share_stack = aco_share_stack_new(0),
    };
    new_server(&server, -1);

    printf("Server is running on localhost:%d\n", server.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(NULL, server.port, 0));

    Cache *cache = 0;

    while (1) {
        int count = epoll_wait(
            server.epoll.fd,
            server.epoll.events,
            server.epoll.len,
            -1
        );

        for (int i = 0; i < count; i++) {
            u64 conn_id = server.epoll.events[i].data.u64;
            u32 events = server.epoll.events[i].events;

            if (conn_id == server.server_id) {
                ConnectionData data = { .cache = &cache, };
                on_connect(&server, connection_main, &data);
                printf("We're launching off, partner.\n");
            } else if (events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                on_disconnect(&server, conn_id);
            } else if (events & EPOLLIN) {
                Connection *c = &server.cs.buf[conn_id];
                aco_resume(c->co);
                if (c->status || c->co->is_end) on_disconnect(&server, conn_id);
            }
        }
    }

    if (close(server.epoll.fd)) err("Failed to close epoll file descriptor.");

    return 0;
}
