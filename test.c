#define EVENT_IMPL
#include "src/event.h"

#include "src/ds.h"

void event_handle_error(EventManager *e) {
    if (e->err.str) {
        fprintf(stderr, "src/event.h:%ld: Error: %s\n", e->err.line, e->err.str);
        exit(1);
    }
}

int main() {
    EventManager event_manager = new_event_manager();
    event_handle_error(&event_manager);

    event_add_read_fd(&event_manager, 0);
    event_handle_error(&event_manager);

    while (1) {
        int count = event_wait(&event_manager, EVENT_SECOND_US);
        event_handle_error(&event_manager);

        for (int i = 0; i < count; i++) {
            int fd = event_get_fd(&event_manager, i);

            if (event_has_error(&event_manager, i)) continue;
            if (!event_is_readable(&event_manager, i)) continue;

            char buf[256];
            ssize_t n = read(fd, buf, arrlen(buf) - 1);

            if (n > 0) {
                buf[n] = '\0';
                printf("Received %zd bytes on FD %d: %s", n, fd, buf);
            } else if (n == 0) {
                printf("Bye!\n");
                event_delete_fd(&event_manager, fd);
                event_handle_error(&event_manager);
                exit(0);
            } else {
                // Read error
                perror("read error");
            }

            // void *udata = event_manager.events[i].udata;
        }
    }

    return 0;
}
