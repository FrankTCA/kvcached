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

typedef struct {
    s8 str;
    int timeout;
} CacheVal;

typedef struct Cache Cache;
typedef struct Cache {
    Cache *child[4];
    bool deleted;
    s8 key;
    CacheVal val;
} Cache;

typedef struct {
    Cache **cache;
} ConnectionData;

Cache *cache_upsert(Cache **c, s8 key, bool make) {
    Cache **least_deleted = 0;

    for (u64 h = s8_hash(key); *c; h <<= 2) {
        if ((*c)->deleted) {
            if (!least_deleted) least_deleted = c;
        } else if (s8_equals(key, (*c)->key)) return *c;
        c = &(*c)->child[h>>62];
    }

    if (!make) return 0;
    if (least_deleted) c = least_deleted;
    else *c = new(0, Cache, 1);
    (*c)->key = s8_copy(0, key);
    (*c)->deleted = false;

    return *c;
}

int cache_delete(Cache **c, s8 key) {
    Cache *e = cache_upsert(c, key, false);
    if (!e) return -1;

    free(e->key.buf);
    free(e->val.str.buf);
    e->key = (s8) {0};
    e->val = (CacheVal) {0};

    e->deleted = true;

    return 0;
}

typedef struct {
    Cache *c;
    int n;
} CacheStackItem;

void cache_clear_all(Cache **c) {
    if (!(*c)) return;

    List(CacheStackItem) stack = { .cap = 128, };

    list_push(&stack, (CacheStackItem) { .c = *c, });

    while (stack.len) {
        CacheStackItem *item = &list_last(&stack);

        for (int i = item->n; i < arrlen(item->c->child); i++) {
            item->n += 1;

            if (item->c->child[i]) {
                list_push(&stack, (CacheStackItem) { .c = item->c->child[i], });
                goto continue_loop;
            }
        }

        free(item->c->key.buf);
        free(item->c->val.str.buf);
        free(item->c);
        stack.len -= 1;

continue_loop:
        continue;
    }

    *c = 0;

    free(stack.buf);
    stack.buf = 0;
}

void connection_main() {
    Connection *c = (Connection *) aco_get_arg();
    ConnectionData *data = (ConnectionData *) c->data;
    Arena key_arena = new_arena(20 * GiB),
          val_arena = { .cap = PTRDIFF_MAX, }; // non-local

    while (!c->status) {
        u64 cmd = recv_i64(&c);
        if (c->status) goto end;

        switch (cmd) {
        case 'S': {
            u64 timeout  = recv_i64(&c);
            s8 key = recv_s8(&key_arena, &c),
               val = recv_s8(&val_arena, &c);
            if (c->status) goto end;

            Cache *entry = cache_upsert(data->cache, key, 1);
            entry->val = (CacheVal) { .str = val, .timeout = timeout, };

            send_s8(&c, s8("OK."));
        } break;
        case 'G': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;

            Cache *entry = cache_upsert(data->cache, key, 0);

            if (entry) send_s8(&c, entry->val.str);
            else send_err(&c, s8("Error: No such key."));
        } break;
        case 'D': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;

            if (cache_delete(data->cache, key)) send_err(&c, s8("Error: No such key."));
            else send_s8(&c, s8("OK."));
        } break;
        case 'C': {
            cache_clear_all(data->cache);
            send_s8(&c, s8("OK."));
        } break;
        case 'T': {
            u64 timeout  = recv_i64(&c);
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;

            Cache *entry = cache_upsert(data->cache, key, 1);
            entry->val.timeout = timeout;

            send_s8(&c, s8("OK."));
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
    Cache cache_t = {0};
    Cache *cache_ptr = &cache_t;
    printf("%ld\n", arrlen(cache_ptr->child));

    aco_thread_init(0);

    Server server = {
        .main_co = aco_create(0, 0, 0, 0, 0),
        .share_stack = aco_share_stack_new(0),
    };
    new_server(&server, -1);

    printf("Server is running on localhost:%d\n", server.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(0, server.port, 0));

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
                printf("Goodbye! Maybe you'll see more next time.\n");
            } else if (events & EPOLLIN) {
                Connection *c = &server.cs.buf[conn_id];
                aco_resume(c->co);
                if (c->status || c->co->is_end) {
                    on_disconnect(&server, conn_id);
                    printf("Bye! See ya on the other side.\n");
                }
            }
        }
    }

    if (close(server.epoll.fd)) err("Failed to close epoll file descriptor.");

    return 0;
}
