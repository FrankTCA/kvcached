#ifndef EVENT_H
#define EVENT_H

#include <sys/event.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#define _event_arrlen(...) (sizeof(__VA_ARGS__) / sizeof(*__VA_ARGS__))
#define EVENT_SECOND_US 1000000
#define _event_set_error(event_manager)            \
    do {                                           \
        (event_manager).err.str = strerror(errno); \
        (event_manager).err.line = __LINE__;       \
    } while (0)

typedef struct kevent PlatformEvent;

typedef struct {
    size_t max_events;
    PlatformEvent events[128];

    struct {
        size_t line;
        char *str;
    } err;

    int fd;
} EventManager;

EventManager new_event_manager() {
    EventManager ret = {0};
    ret.max_events = _event_arrlen(ret.events);

    ret.fd = kqueue();
    if (ret.fd < 0) {
        _event_set_error(ret);
        return ret;
    }

    return ret;
}

void event_add_read_fd(EventManager *e, int fd) {
    (void) e;
    struct kevent k = {0};
    EV_SET(
        &k,
        0,
        EVFILT_READ,
        EV_ADD | EV_ENABLE | EV_CLEAR,
        0, 0, 0
    );

    if (kevent(e->fd, &k, 1, NULL, 0, NULL) == -1) _event_set_error(*e);
}

int event_wait(EventManager *e, int timeout_us) {
    struct timespec timeout = {
        .tv_sec = timeout_us / 1000000,
        .tv_nsec = (timeout_us % 1000000) * 1000,
    };
    size_t count = kevent(e->fd, 0, 0, e->events, e->max_events, &timeout);
    if (count < 0) _event_set_error(*e);
    return count;
}

void event_delete_fd(EventManager *e, int fd) {
    struct kevent k = {0};
    EV_SET(&k, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    if (kevent(e->fd, &k, 1, NULL, 0, NULL) < 0) {

    }
}

int event_get_fd(EventManager *e, size_t i) {
    if (i < 0 || i >= e->max_events) return 0;
    return e->events[i].ident;
}

int event_has_error(EventManager *e, size_t i) {
    if (i < 0 || i >= e->max_events) return 0;
    return e->events[i].flags & EV_ERROR;
}

int event_is_readable(EventManager *e, size_t i) {
    if (i < 0 || i >= e->max_events) return 0;
    return e->events[i].filter == EVFILT_READ;
}

int event_is_writable(EventManager *e, size_t i) {
    if (i < 0 || i >= e->max_events) return 0;
    return e->events[i].filter == EVFILT_WRITE;
}

#endif // EVENT_H

#ifdef EVENT_IMPL
#ifndef EVENT_IMPL_GUARD
#define EVENT_IMPL_GUARD

#endif // EVENT_IMPL_GUARD
#endif // EVENT_IMPL
