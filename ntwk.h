#ifndef NTWK_H
#define NTWK_H

// TODO: see about this
#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

#ifndef NTWK_BLOCKING
#include <libaco/aco.h>
#include <libaco/aco_assert_override.h>
#include <sys/epoll.h>
#endif

#include "ds.h"

#ifndef RECV_MAX_SIZE
#define RECV_MAX_SIZE (1 * GiB) // bytes
#endif // RECV_MAX_SIZE

typedef struct {
    i32 sock;
    int status;
#ifndef NTWK_BLOCKING
    aco_t *co;
#endif
} Connection;

#ifndef NTWK_BLOCKING
typedef struct {
    Connection *buf;
    ssize len;
    ssize cap;
} ConnectionSet;
#endif

typedef struct Server Server;
struct Server {
    u16 port;
    i32 sock;
    struct sockaddr_in addr;
    socklen_t addr_len;
#ifndef NTWK_BLOCKING
    u64 server_id;
    ConnectionSet cs;
    struct {
        int fd;
        ssize len;
        struct epoll_event *events;
    } epoll;
    aco_t *main_co; // If this not null, it's preserved upon new_server(...)
    aco_share_stack_t *share_stack; // Same as above
#endif
};

void new_server(Server *s, i32 server_id);
Connection accept_connection(Server *s);
#ifndef NTWK_BLOCKING
void on_connect(Server *s, aco_cofuncp_t cofn);
#endif

void recv_buf(Connection *c, u8 *buf, int buf_len);
i64 recv_i64(Connection *c);
u8 recv_u8(Connection *c);
s8 recv_s8(Arena *perm, Connection *c);

int send_buf(Connection *c, void *buf, ssize n);
int send_i64(Connection *c, i64 a);
int send_err(Connection *c, s8 str);
int send_s8(Connection *c, s8 str);

#endif // NTWK_H

#ifdef NTWK_IMPL
#ifndef NTWK_IMPL_GUARD
#define NTWK_IMPL_GUARD

#define DS_IMPL
#include "ds.h"

void new_server(Server *s, i32 server_id) {
    assert(server_id < 0);

#ifndef NTWK_BLOCKING
    Server old = *s;
#endif

    *s = (Server) {0};

    s->sock = socket(PF_INET, SOCK_STREAM, 0);
    {
        int yes = 1;
        setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }

    s->addr = (struct sockaddr_in) {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = ntohs(s->port),
    };

    if (bind(s->sock, (struct sockaddr *) &s->addr, sizeof(s->addr))) {
        perror("bind(3) error");
        exit(1);
    }

    s->addr_len = sizeof(s->addr);
    getsockname(s->sock, (struct sockaddr *) &s->addr, &s->addr_len);
    s->port = (int) ntohs(s->addr.sin_port);

    if (listen(s->sock, 1)) {
        perror("listen(3) error");
        exit(1);
    }

#ifndef NTWK_BLOCKING
    s->server_id = server_id;
    s->main_co = old.main_co ? old.main_co : aco_create(0, 0, 0, 0, 0);
    s->share_stack = old.share_stack ? old.share_stack : aco_share_stack_new(0);

    s->cs = (ConnectionSet) { .cap = 64, };
    s->cs.buf = calloc(s->cs.cap, sizeof(*s->cs.buf));

    s->epoll.fd = epoll_create1(0);
    // TODO: handle errors
    if (s->epoll.fd == -1) err("Failed to create epoll file descriptor.");

    {
        s->epoll.len = old.epoll.len ? old.epoll.len : 128;
        s->epoll.events = calloc(s->epoll.len, sizeof(*s->epoll.events));

        struct epoll_event e = {
            .events = EPOLLIN,
            .data.u64 = server_id,
        };
        if (epoll_ctl(s->epoll.fd, EPOLL_CTL_ADD, s->sock, &e)) {
            // TODO: handle errors
            err("Failed to add file descriptor to epoll.");
        }
    }


    int flags = fcntl(s->sock, F_GETFL, 0);
    fcntl(s->sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

#ifndef NTWK_BLOCKING
void on_connect(Server *s, aco_cofuncp_t cofn) {
    // TODO: handle errors

    if (s->cs.len >= s->cs.cap) {
        s->cs.cap *= 2;
        s->cs.buf = realloc(s->cs.buf, sizeof(*s->cs.buf) * s->cs.cap);
    }

    Connection *c = &s->cs.buf[s->cs.len];
    *c = accept_connection(s);

    c->co = aco_create(s->main_co, s->share_stack, 0, cofn, c);

    struct epoll_event e = { .events = EPOLLIN | EPOLLET, };
    e.data.u64 = s->cs.len;
    epoll_ctl(s->epoll.fd, EPOLL_CTL_ADD, c->sock, &e);
    fcntl(c->sock, F_SETFL, O_NONBLOCK);

    s->cs.len += 1;
}
#endif

Connection accept_connection(Server *s) {
    return (Connection) {
        .sock = accept(
            s->sock,
            (struct sockaddr *) &s->addr,
            &s->addr_len
        ),
    };
}

void recv_buf(Connection *c, u8 *buf, int buf_len) {
    ssize_t recvd = 0;

    while (1) {
        // TODO: check for errors
        ssize_t bytes = recv(c->sock, buf, buf_len - recvd, 0);
        recvd += bytes;

        if (bytes < 0) {
            perror("recv(3) error");
            c->status = -1;
            break;
        }

        if (bytes == 0) {
            c->status = 1;
            break;
        }

#ifndef NTWK_BLOCKING
        if (recvd < buf_len) {
            aco_yield();
            continue;
        }
#endif
        break;
    }
}

i64 recv_i64(Connection *c) {
    u8 buf[8] = {0};
    recv_buf(c, buf, arrlen(buf));
    return ((i64) buf[7] <<  0) | ((i64) buf[6] <<  8) |
           ((i64) buf[5] << 16) | ((i64) buf[4] << 24) |
           ((i64) buf[3] << 32) | ((i64) buf[2] << 40) |
           ((i64) buf[1] << 48) | ((i64) buf[0] << 56);
}

u8 recv_u8(Connection *c) {
    u8 ret = 0;
    recv_buf(c, &ret, 1);
    return ret;
}

s8 recv_s8(Arena *perm, Connection *c) {
    s8 ret = {0};
    ret.len = recv_i64(c);

    if (ret.len > RECV_MAX_SIZE) {
        // warning("Packet too large (%ld bytes). Ignoring connection.", ret.len);
        // send_s8(c.sock, s8("Error: Packet cannot be larger than "
        //                                  strify(RECV_MAX_SIZE) " bytes.\n"));
        close(c->sock); // TODO: on_disconnect
        return (s8) {0};
    }

    ret.buf = new(perm, u8, ret.len);
    recv_buf(c, ret.buf, ret.len);

    return ret;
}


int send_buf(Connection *c, void *buf, ssize n) {
    int status = send(c->sock, buf, n, 0);
    if (status < 0) {
        // perror("send(3) error");
        return status;
    }
    return 0;
}

int send_i64(Connection *c, i64 a) {
    u8 buf[8];
    buf[7] = (a & 0x00000000000000ff);
    buf[6] = (a & 0x000000000000ff00) >> 8;
    buf[5] = (a & 0x0000000000ff0000) >> 16;
    buf[4] = (a & 0x00000000ff000000) >> 24;
    buf[3] = (a & 0x000000ff00000000) >> 32;
    buf[2] = (a & 0x0000ff0000000000) >> 40;
    buf[1] = (a & 0x00ff000000000000) >> 48;
    buf[0] = (a & 0xff00000000000000) >> 56;
    return send_buf(c, buf, sizeof(buf));
}

int send_err(Connection *c, s8 str) {
    int status = send_i64(c, -str.len);
    if (status) return status;

    return send_buf(c, str.buf, str.len * sizeof(u8));
}

int send_s8(Connection *c, s8 str) {
    int status = send_i64(c, str.len);
    if (status) return status;
    return send_buf(c, str.buf, str.len * sizeof(u8));
}

#endif // NTWK_IMPL_GUARD
#endif // NTWK_IMPL
