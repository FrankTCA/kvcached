#ifndef EVENT_H
#define EVENT_H

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include <unistd.h>

#ifdef EVENT_DETECT_PLATFORM
#ifdef __linux__
#define EVENT_USE_EPOLL
#else
#define EVENT_USE_KQUEUE
#endif
#endif

#ifndef EVENT_MAX_ERR_STR_LEN
#define EVENT_MAX_ERR_STR_LEN 256
#endif

#define EVENT_SECOND_US 1000000
#define _event_set_error(event_manager)       \
    do {                                      \
        (event_manager)->err.line = __LINE__; \
        strerror_r(                           \
            errno,                            \
            ((event_manager)->err.str),       \
            EVENT_MAX_ERR_STR_LEN             \
       );                                     \
    } while (0)

typedef enum {
    EVENT_FLAG_READ  = 1 << 0,
    EVENT_FLAG_WRITE = 1 << 1,
} EventFlags;

#ifdef EVENT_USE_EPOLL
#include <sys/epoll.h> 
typedef struct epoll_event PlatformEvent;
#elif EVENT_USE_KQUEUE
#include <sys/event.h> 
typedef struct kevent PlatformEvent;
#endif

typedef struct {
    int fd, is_terminated, is_writable, is_readable;
    void *data;
} PolledEvent;

typedef struct {
    size_t event_throughput;
    PlatformEvent *events;
    struct {
        size_t line;
        char str[EVENT_MAX_ERR_STR_LEN];
    } err;
    int fd;
    struct {
        char *buf;
        size_t cap;
    } mem;
} EventManager;

ptrdiff_t new_event_manager(EventManager *e, int event_throughput);
int new_event_manager_alloc(EventManager *e, int throughput, void *(*alloc)(size_t));
void event_use_memory(EventManager *e);
int event_add_fd(EventManager *e, int fd, EventFlags flags, void *data);
int event_change_data(EventManager *e, int fd, EventFlags flags, void *data);
int event_wait(EventManager *e, int timeout_us);
int event_delete_fd(EventManager *e, int fd);
void event_manager_close(EventManager *e);
PolledEvent get_polled_event(EventManager *e, size_t i);

#endif // EVENT_H

#ifdef EVENT_IMPL
#ifndef EVENT_IMPL_GUARD
#define EVENT_IMPL_GUARD

ptrdiff_t new_event_manager(EventManager *e, int event_throughput) {
    e->event_throughput = event_throughput;
    e->mem.cap = event_throughput * sizeof(*e->events);

#ifdef EVENT_USE_EPOLL
    e->fd = epoll_create1(0);
#elif EVENT_USE_KQUEUE
    e->fd = kqueue();
#endif

    if (e->fd < 0) {
        _event_set_error(e);
        return -1;
    }

    return e->mem.cap;
}

int new_event_manager_alloc(EventManager *e, int throughput, void *(*alloc)(size_t)) {
    ptrdiff_t cap = new_event_manager(e, throughput);
    if (cap < 0) return -1;
    e->mem.buf = alloc(cap);
    if (!e->mem.buf) return -1;
    memset(e->mem.buf, 0, cap);
    event_use_memory(e);
    return 0;
}

void event_use_memory(EventManager *e) {
    e->events = (PlatformEvent *) e->mem.buf;
}

int event_add_fd(EventManager *e, int fd, EventFlags flags, void *data) {
#ifdef EVENT_USE_EPOLL
    uint32_t eflags  = 0;

    if (flags & EVENT_FLAG_READ) eflags |= EPOLLIN;
    if (flags & EVENT_FLAG_WRITE) eflags |= EPOLLOUT;

    struct epoll_event event = {
        .events = eflags,
        .data.ptr = data,
    };

    if (epoll_ctl(e->fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        _event_set_error(e);
        return -1;
    }
#elif EVENT_USE_KQUEUE
    if (flags & EVENT_FLAG_READ) {
        struct kevent k = {0};
        EV_SET(
            &k,
            fd,
            EVFILT_READ,
            EV_ADD | EV_ENABLE,
            0, 0, data
        );

        if (kevent(e->fd, &k, 1, NULL, 0, NULL) == -1) {
            _event_set_error(e);
            return -1;
        }
    }

    if (flags & EVENT_FLAG_WRITE) {
        struct kevent k = {0};
        EV_SET(
            &k,
            fd,
            EVFILT_WRITE,
            EV_ADD | EV_ENABLE,
            0, 0, data
        );

        if (kevent(e->fd, &k, 1, NULL, 0, NULL) == -1) {
            _event_set_error(e);
            return -1;
        }
    }
#endif

    return 0;
}

int event_change_data(EventManager *e, int fd, EventFlags flags, void *data) {
#ifdef EVENT_USE_EPOLL
    uint32_t eflags  = 0;

    if (flags & EVENT_FLAG_READ) eflags |= EPOLLIN;
    if (flags & EVENT_FLAG_WRITE) eflags |= EPOLLOUT;

    struct epoll_event event = {
        .events = eflags,
        .data.ptr = data, 
    };

    if (epoll_ctl(e->fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        _event_set_error(e);
        return -1;
    }
#elif EVENT_USE_KQUEUE
    if (flags & EVENT_FLAG_READ) {
        struct kevent k = {0};
        EV_SET(
            &k,
            fd,
            EVFILT_READ,
            EV_ADD | EV_ENABLE,
            0, 0, data
        );

        if (kevent(e->fd, &k, 1, NULL, 0, NULL) == -1) {
            _event_set_error(e);
            return -1;
        }
    }

    if (flags & EVENT_FLAG_WRITE) {
        struct kevent k = {0};
        EV_SET(
            &k,
            fd,
            EVFILT_WRITE,
            EV_ADD | EV_ENABLE,
            0, 0, data
        );

        if (kevent(e->fd, &k, 1, NULL, 0, NULL) == -1) {
            _event_set_error(e);
            return -1;
        }
    }
#endif

    return 0;
}

int event_wait(EventManager *e, int timeout_us) {
#ifdef EVENT_USE_EPOLL
    int timeout_ms = timeout_us / 1000;
    int count = epoll_wait(
        e->fd,
        e->events,
        e->event_throughput,
        timeout_ms
    );
#elif EVENT_USE_KQUEUE
    struct timespec timeout = {
        .tv_sec = timeout_us / EVENT_SECOND_US,
        .tv_nsec = (timeout_us % EVENT_SECOND_US) * 1000,
    };
    int count = kevent(
        e->fd,
        NULL, 
        0,
        e->events,
        e->event_throughput,
        &timeout
    );
#endif

    if (count < 0) {
        _event_set_error(e);
        return -1;
    }
    return count;
}

int event_delete_fd(EventManager *e, int fd) {
#ifdef EVENT_USE_EPOLL
    if (epoll_ctl(e->fd, EPOLL_CTL_DEL, fd, NULL) == -1) { 
        _event_set_error(e);
        return -1;
    }
#elif EVENT_USE_KQUEUE
    struct kevent k = {0};
    // TODO: EVFILT_READ and EVFILT_WRITE
    EV_SET(&k, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    if (kevent(e->fd, &k, 1, NULL, 0, NULL) < 0) {
        _event_set_error(e);
        return -1;
    }
#endif

    return 0;
}

void event_manager_close(EventManager *e) {
    if (e->fd > 0) {
        close(e->fd);
        e->fd = 0;
    }
}

PolledEvent get_polled_event(EventManager *e, size_t i) {
    if (i >= e->event_throughput) {
        return (PolledEvent) { .fd = -1, };
    }

#ifdef EVENT_USE_EPOLL
    return (PolledEvent) {
        .fd = e->events[i].data.fd, 
        .data = e->events[i].data.ptr,
        .is_terminated = e->events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP),
        .is_readable = e->events[i].events & EPOLLIN,
        .is_writable = e->events[i].events & EPOLLOUT,
    };
#elif EVENT_USE_KQUEUE
    return (PolledEvent) {
        .fd = e->events[i].ident,
        .data = e->events[i].udata,
        .is_terminated = e->events[i].flags & (EV_ERROR | EV_EOF),
        .is_readable = e->events[i].filter == EVFILT_READ,
        .is_writable = e->events[i].filter == EVFILT_WRITE,
    };
#endif
}

#endif // EVENT_IMPL_GUARD
#endif // EVENT_IMPL
