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
    *((int *) 0) = 0; \
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
    s8 str;
    int buf_len;
    int status;
} Endpoint;

void recv_buf() {
    Endpoint *c = (Endpoint *) aco_get_arg();

    ssize_t recvd = 0;

    while (1) {
        ssize_t bytes = recv(c->sock, c->str.buf + recvd, buf_len - recvd, 0);

        if (bytes < 0) {
            perror("recv(3) error");
            *status = -1;
            goto end;
        }

        if (bytes == 0) {
            *status = 1;
            goto end;
        }

        recvd += bytes;

        if (recvd < ->buf_len) {
            aco_yield();
            continue;
        } else goto end;
    }

end:
    aco_exit();
}

int main() {
    // aco_thread_init(NULL);
    // aco_t *main_co = aco_create(NULL, NULL, 0, NULL, NULL);

    // aco_share_stack_t *sstk = aco_share_stack_new(0);

    // int shared_counter = 0;
    // aco_t *co = aco_create(main_co, sstk, 0, count_for_me, &shared_counter);

    // int ct = 0;
    // while (ct < 6) {
    //     aco_resume(co);
    //     // printf("main_co: %p\n", main_co);
    //     printf("I am main %d!!!\n", shared_counter);
    //     ct += 1;
    // }

    // // printf("main_co: %p\n", main_co);
    // printf("I am done.\n");

    // aco_destroy(co);
    // co = NULL;
    // aco_share_stack_destroy(sstk);
    // sstk = NULL;
    // aco_destroy(main_co);
    // main_co = NULL;

    // while (1) {

    // }

    // Arena perm = new_arena(2 * KiB);
    // int status = 0;
    // s8 msg = recv_s8(&perm, sock, &status);
    // s8_print(msg);
    // printf("%ld\n", msg.len);

    Arena scratch = new_arena(4 * KiB);

    Server s = {0};
    new_server(&s);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) err("Failed to create epoll file descriptor.");

    struct epoll_event event = { .events = EPOLLIN, },
                       events[128] = {0};

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, s.sock, &event)) {
        err("Failed to add file descriptor to epoll.");
    }

    printf("Server is running on localhost:%d\n", s.port);
    s8_write_to_file(s8("port.txt"), u64_to_s8(&scratch, s.port, 0));

    while (1) {
        printf("Polling for input...\n");
        int count = epoll_wait(epoll_fd, events, arrlen(events), -1);
        printf("%d ready events.\n", count);

        for (int i = 0; i < count; i++) {
            if (events[i].data.fd == s.sock) {
                // TODO: handle errors
                struct epoll_event e = { .events = EPOLLIN | EPOLLET, };
                e.data.fd = accept(
                    s.sock,
                    (struct sockaddr *) &s.addr,
                    &s.addr_len
                );
                fcntl(e.data.fd, F_SETFL, O_NONBLOCK);
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, e.data.fd, &e);
                printf("Accepted new connection on socket %d\n", e.data.fd);
            } else {

            }

            // int n = read(events[i].data.fd, scratch.buf, scratch.cap);
            // printf("%s\n", scratch.buf);
        }

    }

    if (close(epoll_fd)) err("Failed to close epoll file descriptor.");
}
