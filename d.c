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

    free(scratch.buf);
    scratch = (Arena) {0};

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
                on_connect(&server, connection_main);
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
}
