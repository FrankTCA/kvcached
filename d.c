#include <libaco/aco.h>
#include <libaco/aco_assert_override.h>

#define DS_IMPL
#include "ds.h"

#define NTWK_IMPL
#include "ntwk.h"

#include <sys/epoll.h>

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

// void count_for_me() {
//     int *iretp = (int *) aco_get_arg();

//     aco_t *this_co = aco_get_co();

//     int ct = 0;
//     while (ct < 6) {
//         // printf("co: %p save_stack: %p share_stack: %p yield_ct: %d\n",
//         //     this_co, this_co->save_stack.ptr,
//         //     this_co->share_stack->ptr, ct
//         // );
//         printf("I'm in this loop. The ct is %d\n", ct);
//         aco_yield();
//         (*iretp)++;
//         ct++;
//     }

//     printf("I'm out of this loop. The ct is %d\n", ct);

//     // printf("co: %p save_stack %p share_stack %p co_exit()\n",
//     //     this_co, this_co->save_stack.ptr,
//     //     this_co->share_stack->ptr
//     // );

//     aco_exit();
// }

typedef struct {
    i32 sock;
    Arena buf;
    int buf_len;
    int status;
    aco_t *co;
} Connection;

typedef struct {
    Connection *buf;
    ssize len;
    ssize cap;
    aco_t *main_co;
    aco_share_stack_t *share_stack;
} ConnectionSet;

void recv_buf() {
    Connection *c = (Connection *) aco_get_arg();

    ssize_t recvd = 0;

    while (1) {
        ssize_t bytes = recv(c->sock, &c->buf.buf[c->buf.len], c->buf_len - recvd, 0);
        c->buf.len += bytes; // TODO: check if it exceeded the capacity
        recvd += bytes;

        if (bytes < 0) {
            perror("recv(3) error");
            c->status = -1;
            goto end;
        }

        if (bytes == 0) {
            c->status = 1;
            goto end;
        }

        {
            printf("\"");
            s8_print((s8) { .buf = c->buf.buf, .len = c->buf.len, });
            printf("\"\n");
        }

        if (recvd < c->buf_len) aco_yield();
        else goto end;
    }

end:
    aco_exit();
}

void on_connect(Server *s, ConnectionSet *cs) {
    // TODO: handle errors

    struct epoll_event e = { .events = EPOLLIN | EPOLLET, };

    e.data.u64 = cs->len++;

    if (cs->len > cs->cap) {
        cs->cap *= 2;
        cs->buf = realloc(cs->buf, sizeof(*cs->buf) * cs->cap);
    }

    Connection *c = &cs->buf[cs->len - 1];
    *c = (Connection) {
        .sock = e.data.fd,
        .buf = new_arena(1 * KiB),
        .buf_len = 5,
    };
    c->co = aco_create(cs->main_co, cs->share_stack, 0, recv_buf, c);

    c->sock = accept(
        s->sock,
        (struct sockaddr *) &s->addr,
        &s->addr_len
    );

    epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, c->sock, &e);
    fcntl(e.data.fd, F_SETFL, O_NONBLOCK);

    printf("Accepted new connection on socket %d\n", e.data.fd);
}

// void on_disconnect(ConnectionSet *cs, int sock) {
//     cs->
// }

int main() {
    const u64 SERVER_ID = -1;

    aco_thread_init(NULL);

    // aco_destroy(co);
    // co = NULL;
    // aco_share_stack_destroy(sstk);
    // sstk = NULL;
    // aco_destroy(main_co);
    // main_co = NULL;

    // Arena perm = new_arena(2 * KiB);
    // int status = 0;
    // s8 msg = recv_s8(&perm, sock, &status);
    // s8_print(msg);
    // printf("%ld\n", msg.len);

    Arena scratch = new_arena(4 * KiB);

    Server server = {0};
    new_server(&server);

    ConnectionSet connection_set = { .cap = 64, };
    connection_set.buf = calloc(connection_set.cap, sizeof(*connection_set.buf));

    connection_set.main_co = aco_create(0, 0, 0, 0, 0);
    connection_set.share_stack = aco_share_stack_new(0);

    server.epoll_fd = epoll_create1(0);
    if (server.epoll_fd == -1) err("Failed to create epoll file descriptor.");

    struct epoll_event events[128] = {0};

    {
        struct epoll_event e = {
            .events = EPOLLIN,
            .data.u64 = SERVER_ID, 
        };
        if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, server.sock, &e)) {
            err("Failed to add file descriptor to epoll.");
        }
    }


    printf("Server is running on localhost:%d\n", server.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(&scratch, server.port, 0));

    while (1) {
        int count = epoll_wait(server.epoll_fd, events, arrlen(events), -1);

        for (int i = 0; i < count; i++) {
            if (events[i].data.u64 == SERVER_ID) {
                on_connect(&server, &connection_set);
            } else {
                Connection *c = &connection_set.buf[events[i].data.u64];
                printf("READMEEEEE on socket %d!\n", c->sock);
                aco_resume(c->co);
            }

            // int n = read(events[i].data.fd, scratch.buf, scratch.cap);
            // printf("%server\n", scratch.buf);
        }

    }

    if (close(server.epoll_fd)) err("Failed to close epoll file descriptor.");
}
