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

void connection_main() {
    Connection *c = (Connection *) aco_get_arg();
    Arena scratch = new_arena(2 * KiB);
    s8 r = recv_s8(&scratch, c);
    s8_print(r);
    aco_exit();
}

int main() {
    aco_thread_init(NULL);

    // aco_destroy(co);
    // co = NULL;
    // aco_share_stack_destroy(sstk);
    // sstk = NULL;
    // aco_destroy(main_co);
    // main_co = NULL;

    Arena scratch = new_arena(4 * KiB);

    Server server = {
        .main_co = aco_create(0, 0, 0, 0, 0),
        .share_stack = aco_share_stack_new(0),
    };
    new_server(&server, -1);

    printf("Server is running on localhost:%d\n", server.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(&scratch, server.port, 0));
    printf("%ld\n", server.epoll.len);

    while (1) {
        int count = epoll_wait(
            server.epoll.fd,
            server.epoll.events,
            server.epoll.len,
            -1
        );

        for (int i = 0; i < count; i++) {
            if (server.epoll.events[i].data.u64 == server.server_id) {
                on_connect(&server, connection_main);
            } else {
                Connection *c = &server.cs.buf[server.epoll.events[i].data.u64];
                printf("READMEEEEE on socket %d!\n", c->sock);
                aco_resume(c->co);
            }
        }
    }

    if (close(server.epoll.fd)) err("Failed to close epoll file descriptor.");
}
