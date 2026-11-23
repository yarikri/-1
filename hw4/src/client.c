// client.c - клиент берега или наблюдатель
#include "net.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
#ifdef WATCH
    char role = 'O';
    if (argc != 3) { dprintf(2, "Запуск: %s IP порт\n", argv[0]); return 2; }
#else
    if (argc != 4 || (strcmp(argv[3], "left") && strcmp(argv[3], "right"))) {
        dprintf(2, "Запуск: %s IP порт left|right\n", argv[0]); return 2;
    }
    char role = argv[3][0] == 'l' ? 'L' : 'R';
#endif
    int fd = tcp_open(argv[1], argv[2], 0);
    if (fd < 0) return 2;
    int rc = 0;
    if (send(fd, &role, 1, MSG_NOSIGNAL) != 1) rc = 2;
    char c, line[1024];
    size_t n = 0;
    ssize_t got = 0;
    while (!rc && (got = read(fd, &c, 1)) != 0) {
        if (got < 0) { if (errno == EINTR) continue; rc = 2; break; }
        if (n == sizeof line - 1) { rc = 2; break; }
        line[n++] = c;
        if (c != '\n') continue;
        line[n] = 0;
        if (role != 'O' && !strcmp(line, "?\n")) {
            if (send(fd, "A", 1, MSG_NOSIGNAL) != 1) rc = 2;
        } else dprintf(1, "%s", line);
        n = 0;
    }
    close(fd);
    return rc;
}
