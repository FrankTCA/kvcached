#define EVENT_IMPL
#define EVENT_DETECT_PLATFORM
#include "src/event.h"

#include "src/ds.h"

void event_handle_error(EventManager *e) {
    if (e->err.str[0]) {
        fprintf(stderr, "src/event.h:%ld: Error: %s\n", e->err.line, e->err.str);
        exit(1);
    }
}

int main() {
    EventManager event_manager = new_event_manager(1024);
    event_manager.events = calloc(
        event_manager.event_throughput_count,
        sizeof(PlatformEvent)
    );

    event_add_fd(&event_manager, 0, EVENT_FLAG_READ);
    event_handle_error(&event_manager);

    while (1) {
        int count = event_wait(&event_manager, EVENT_SECOND_US);
        event_handle_error(&event_manager);

        for (int i = 0; i < count; i++) {
            PolledEvent event = get_polled_event(&event_manager, i);

            if (event.is_terminated) continue;
            if (!event.is_readable) continue;

            char buf[256];
            ssize_t n = read(event.fd, buf, arrlen(buf) - 1);

            if (n > 0) {
                buf[n] = '\0';
                printf("Received %zd bytes on FD %d: %s", n, event.fd, buf);
            } else if (n == 0) {
                printf("Bye!\n");
                event_delete_fd(&event_manager, event.fd, EVENT_FLAG_READ);
                event_handle_error(&event_manager);
                exit(0);
            } else {
                perror("read error");
            }
        }
    }

    return 0;
}
