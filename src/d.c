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

#include <time.h>

typedef struct {
    s8 str;
    u64 timeout, last_accessed;
} CacheVal;

typedef struct CacheItem CacheItem;
typedef struct CacheItem {
    ssize child[4];
    bool deleted;
    s8 key;
    CacheVal val;
} CacheItem;

typedef struct {
    List(CacheItem) set;
} Cache;

ssize cache_upsert_index(Cache *cache, s8 key, bool make) {
    ssize least_deleted = -1, node = 0, parent = 0;
    u64 i = 0;

    bool initial = true;
    for (u64 h = s8_hash(key); (node || initial) && cache->set.len; h <<= 2) {
        initial = false;
        CacheItem *c = &cache->set.buf[node];

        if (c->deleted) {
            if (least_deleted < 0) least_deleted = node;
        } else if (s8_equals(key, c->key)) return node;

        parent = node;
        i = h >> 62;
        node = c->child[i];
    }

    if (!make) return -1;

    if (least_deleted >= 0) node = least_deleted;
    else {
        list_push(&cache->set, (CacheItem) {0});
        node = cache->set.len - 1;
    }

    cache->set.buf[parent].child[i] = node;

    CacheItem *c = &cache->set.buf[node];
    c->key = s8_copy(0, key);
    c->val = (CacheVal) {0};
    c->deleted = false;

    return node;
}

CacheItem *cache_upsert(Cache *cache, s8 key, bool make) {
    ssize node = cache_upsert_index(cache, key, make);
    if (node < 0) return 0;
    return &cache->set.buf[node];
}

void cache_delete_item(CacheItem *c) {
    free(c->key.buf);
    free(c->val.str.buf);
    c->key = (s8) {0};
    c->val = (CacheVal) {0};
    c->deleted = true;
}

int cache_delete(Cache *cache, s8 key) {
    ssize node = cache_upsert_index(cache, key, false);
    if (node < 0) return -1;

    cache_delete_item(&cache->set.buf[node]);

    return 0;
}

int cache_clear_all(Cache *cache) {
    for (int i = 0; i < cache->set.len; i++) {
        CacheItem *c = &cache->set.buf[i];
        free(c->key.buf);
        free(c->val.str.buf);
    }

    free(cache->set.buf);
    cache->set.buf = 0;
    cache->set.len = 0;

    return 0;
}

typedef struct {
    Cache *cache;
} ConnectionData;

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

            CacheItem *item = cache_upsert(data->cache, key, 1);
            item->val = (CacheVal) {
                .str = val,
                .timeout = timeout,
                .last_accessed = time(0),
            };

            send_s8(&c, s8("OK."));
        } break;
        case 'G': {
            s8 key = recv_s8(&key_arena, &c);
            if (c->status) goto end;

            CacheItem *item = cache_upsert(data->cache, key, 0);

            if (item) {
                send_s8(&c, item->val.str);
                item->val.last_accessed = time(0);
            } else send_err(&c, s8("Error: No such key."));
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

            CacheItem *item = cache_upsert(data->cache, key, 1);
            if (item) {
                item->val.timeout = timeout;
                send_s8(&c, s8("OK."));
            } else send_err(&c, s8("Error: No such key."));
        } break;
        default: {
            send_s8(&c, s8("Error: No such command."));
        }
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
    aco_thread_init(0);

    Server server = {
        .main_co = aco_create(0, 0, 0, 0, 0),
        .share_stack = aco_share_stack_new(0),
    };
    new_server(&server, -1);

    printf("Server is running on localhost:%d\n", server.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(0, server.port, 0));

    Cache cache = { .set.cap = 128, };

    while (1) {
        int count = event_wait(&server.event_manager, EVENT_SECOND_US);

        bool alive = 0;
        for (int i = 0; i < cache.set.len; i++) {
            CacheItem *c = &cache.set.buf[i];

            if (c->deleted) continue;
            alive = true;
            if (!c->val.timeout) continue;

            if (time(NULL) - c->val.last_accessed >= c->val.timeout) {
                cache_delete_item(c);
            }
        }

        if (!alive) cache_clear_all(&cache);

        // TODO: see about epoll flags
        // TODO: make get_conn_id function

        for (int i = 0; i < count; i++) {
            PolledEvent event = get_polled_event(&server.event_manager, i);
            u64 conn_id = (u64) event.data;

            if (conn_id == server.server_id) {
                ConnectionData data = { .cache = &cache, };
                on_connect(&server, connection_main, &data);
                printf("We're launching off, partner.\n");
            } else if (event.is_terminated) {
                on_disconnect(&server, conn_id);
                printf("Goodbye! Maybe you'll see more next time.\n");
            } else if (event.is_readable) {
                Connection *c = &server.cs.buf[conn_id];
                aco_resume(c->co);
                // TODO: see if c->status should silently fail some of the time
                if (c->status || c->co->is_end) {
                    on_disconnect(&server, conn_id);
                    printf("Bye! See ya on the other side.\n");
                }
            }
        }
    }

    event_manager_close(&server.event_manager);

    return 0;
}

/* TODO:
   [x]: Efficient and correct networking
   [x]: "Get/set/delete" commands
   [x]: "Clear all" command
   [x]: Delete key on timeout
   [ ]: Clear everything on timeout
   [x]: FreeBSD support
   [ ]: Publish program to ports
   [ ]: Encryption / password lock
   [ ]: Chroot stuff
   [ ]: Configuration
   [ ]: Bind address
   [ ]: Max memory behavior
*/ 
