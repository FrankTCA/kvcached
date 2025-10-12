#define DS_IMPL
#include "ds.h"

#define NTWK_BLOCKING
#define NTWK_IMPL
#include "ntwk.h"

#define ARGP_IMPL
#include "argp.h"

#define SET       "set"
#define SET_U     "<timeout> <key> <value>"
#define SET_D     "Sets with a timeout (see the timeout command) " \
                  "the key to the value "

#define GET       "get"
#define GET_U     "<key>"
#define GET_D     "Prints the value of the given key."

#define DELETE    "delete"
#define DELETE_U  "<key>"
#define DELETE_D  "Deletes the given key."

#define CLEAR     "clear"
#define CLEAR_U   ""
#define CLEAR_D   "Clears the cache."

#define TIMEOUT   "timeout"
#define TIMEOUT_U "<key> <seconds>"
#define TIMEOUT_D "Sets the timeout of the given key. Every time a key " \
                  "is accessed, the timer on that key is reset. " \
                  "If the timer ends, the key will be deleted. " \
                  "A timeout of zero disables the timer."

void usage_err(ArgParser a) {
    fprintf(stderr, "Usage: %s <port> ", a.argv[0]);
    s8_fprint(stderr, a.extra_usage);
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "    \"" SET " " SET_U "\" - " SET_D "\n");
    fprintf(stderr, "    \"" GET " " GET_U "\" - " GET_D "\n");
    fprintf(stderr, "    \"" DELETE " " DELETE_U "\" - " DELETE_D "\n");
    fprintf(stderr, "    \"" CLEAR " " CLEAR_U "\" - " CLEAR_D "\n");
    fprintf(stderr, "    \"" TIMEOUT " " TIMEOUT_U "\" - " TIMEOUT_D "\n");
    exit(1);
}

Connection connect_to_localhost(u16 port) {
    Connection ret = { .sock = socket(PF_INET, SOCK_STREAM, 0), };

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short) port);

    char addrstr[NI_MAXHOST + NI_MAXSERV + 1];
    snprintf(addrstr, sizeof(addrstr), "127.0.0.1:%d", port);

    inet_pton(AF_INET, addrstr, &addr.sin_addr);

    if (connect(ret.sock, (struct sockaddr*) &addr, sizeof(addr))) {
        perror("connect(3) error");
        exit(1);
    }

    return ret;
}

int main(int argc, char *argv[]) {
    ArgParser argp = {
        .argc = argc, .argv = argv,
        .i = 1,
        .extra_usage = s8("<command> <parameters>"),
        .usage_err = usage_err,
    };

    u16 port;
    {
        s8 p = get_next_arg(&argp);
        port = s8_to_u64(p);
        if (port == 0) argp.usage_err(argp);
    }

    s8 command = get_next_arg(&argp);

    Arena perm = new_arena(2 * KiB);

    Connection _conn = connect_to_localhost(port);
    Connection *conn = &_conn;

    if (s8_equals(command, s8(SET))) {
        argp.extra_usage = s8(SET " " SET_U);
        u64 timeout = s8_to_u64(get_next_arg(&argp));
        s8 key = get_next_arg(&argp);
        s8 val = get_next_arg(&argp);
            send_i64(&conn, 'S');
            send_i64(&conn, timeout);
            send_s8(&conn, key);
            send_s8(&conn, val);
        no_more_args(argp);
    } else if (s8_equals(command, s8(GET))) {
        argp.extra_usage = s8(GET " " GET_U);
        s8 key = get_next_arg(&argp);
            send_i64(&conn, 'G');
            send_s8(&conn, key);
        no_more_args(argp);
    } else if (s8_equals(command, s8(DELETE))) {
        argp.extra_usage = s8(DELETE " " DELETE_U);
        s8 key = get_next_arg(&argp);
            send_i64(&conn, 'D');
            send_s8(&conn, key);
        no_more_args(argp);
    } else if (s8_equals(command, s8(CLEAR))) {
        argp.extra_usage = s8(CLEAR " " CLEAR_U);
            send_i64(&conn, 'C');
        no_more_args(argp);
    } else if (s8_equals(command, s8(TIMEOUT))) {
        argp.extra_usage = s8(TIMEOUT " " TIMEOUT_U);
        u64 timeout = s8_to_u64(get_next_arg(&argp));
        s8 key = get_next_arg(&argp);
            send_i64(&conn, 'T');
            send_i64(&conn, timeout);
            send_s8(&conn, key);
        no_more_args(argp);
    } else argp.usage_err(argp);

    Arena a = new_arena(12 * GiB);
    s8 r = recv_s8(&a, &conn);
    s8_print(r);
    printf("\n");

    return 0;
}
