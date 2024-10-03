#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

char keys[1024][1024];
char values[1024][1024];
int num_keys;
unsigned long memory_max;
unsigned long memory_used;
int enable = 1;

unsigned long seconds = 0;

unsigned long times[1024];
char* keys_time[1024];
int times_count;

void init_cache(unsigned long mem) {
    unsigned long memory_div = mem / 1024;
    num_keys = 0;
    memory_max = mem;
    memory_used = 0;
}

int save_var(char* key, char* value) {
    if (memory_used + strlen(value) > memory_max) {
        printf("Max memory used!\n");
        return 1;
    }
    strncpy(keys[num_keys], key, 1024);
    strncpy(values[num_keys], value, 1024);
    memory_used += strlen(value);
    num_keys++;
    return 0;
}

char* load_var(const char* key) {
    for (int i = 0; i < num_keys; i++) {
        if (strcmp(keys[i], key) == 0) {
            return values[i];
        }
    }
    return NULL;
}

int delete_var(char* key) {
    for (int i = 0; i < num_keys; i++) {
        if (strcmp(keys[i], key) == 0) {
            memory_used -= strlen(values[i]);
            num_keys--;
            for (int index = i; index < num_keys - 1; index++) {
                strncpy(keys[index], keys[index + 1], 1024);
                strncpy(values[index], values[index + 1], 1024);
                return 1;
            }
        }
    }
    return 0;
}

void clear_cache() {
    num_keys = 0;
    memory_used = 0;
    init_cache(memory_max);
}

void sigalrm_handler(int sig) {
    seconds++;
    for (int i = 0; i < times_count; i++) {
        if (seconds == times[i]) {
            delete_var(keys_time[i]);
            times_count--;
            for (int index = i; index < times_count - 1; index++) {
                times[index] = times[index + 1];
                keys_time[index] = keys_time[index + 1];
            }
        }
    }
}

int open_port(int port) {
    char protoname[] = "TCP";
    struct protoent *protoent;
    int i;
    int newline_found = 0;
    int server_sockfd, client_sockfd;
    socklen_t client_len;
    ssize_t n_bytes_read;
    struct sockaddr_in client_addr, server_addr;
    unsigned short server_port = port;

    protoent = getprotobyname(protoname);
    if (protoent == NULL) {
        printf("getprotobyname failed\n");
        return 1;
    }
    server_sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (server_sockfd < 0) {
        printf("Could not create socket!\n");
        return 1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Could not bind to socket!\n");
        return 1;
    }

    if (listen(server_sockfd, 5) < 0) {
        printf("Server cannot listen!\n");
        return 1;
    }

    printf("Listening on port %d\n", server_port);

    while (enable) {
        client_len = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        char* packet = "";
        int response = 0;
        char buffer[BUFSIZ];
        while (n_bytes_read = read(client_sockfd, buffer, BUFSIZ)) {
                if (buffer[0] == 'S') {
                    write(client_sockfd, "SENDVAL", 7);
                    char buf2[BUFSIZ];
                    if (n_bytes_read = read(client_sockfd, buf2, BUFSIZ)) {
                        if (save_var(buffer+2, buf2)) {
                            printf("Could not save key %s!\n", buffer+2);
                            write(client_sockfd, "ERRMEM", 6);
                            close(client_sockfd);
                            memset(buffer, 0, sizeof(buffer));
                            memset(buf2, 0, sizeof(buf2));
                            break;
                        }
                        printf("Saved key %s with value %s\n", buffer+2, buf2);
                        write(client_sockfd, "OK", 2);
                        close(client_sockfd);
                        memset(buffer, 0, sizeof(buffer));
                        memset(buf2, 0, sizeof(buf2));
                        break;
                    }
                } else if (buffer[0] == 'G') {
                    char* value = load_var(buffer+2);
                    if (value == NULL) {
                        printf("Could not load key %s!\n", buffer+2);
                        write(client_sockfd, "ERRNULL", 7);
                        close(client_sockfd);
                        memset(buffer, 0, sizeof(buffer));
                        break;
                    }
                    printf("Loaded key %s, value %s\n", buffer+2, value);
                    write(client_sockfd, value, strlen(value));
                    close(client_sockfd);
                    memset(buffer, 0, sizeof(buffer));
                    break;
                } else if (buffer[0] == 'D') {
                    delete_var(buffer+2);
                    printf("Deleted key %s\n", buffer+2);
                    write(client_sockfd, "OK", 2);
                    close(client_sockfd);
                    memset(buffer, 0, sizeof(buffer));
                    break;
                } else if (buffer[0] == 'C') {
                    clear_cache();
                    write(client_sockfd, "OK", 2);
                    close(client_sockfd);
                    memset(buffer, 0, sizeof(buffer));
                    break;
                } else {
                    printf("Client sent unrecognized request!\n");
                    write(client_sockfd, "ERRBADREQ", 9);
                    close(client_sockfd);
                    break;
                }
            }
        close(client_sockfd);
    }
    return 0;
}


int main(int argc, char** argv) {
    printf("Initializing cache...\n");
    if (argc < 2) {
        printf("Usage: kvcached <port> <memory>\n");
        return 1;
    }

    int port = atoi(argv[1]);
    unsigned long memory = atol(argv[2]);
    init_cache(memory);
    signal(SIGALRM, sigalrm_handler);
    ualarm(1000000, 1000000);
    return open_port(port);
}
