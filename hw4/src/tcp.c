// tcp.c - адрес и порт задаются при запуске
#include "net.h"
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

int tcp_open(const char *ip, const char *port, int server) {
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM,
                              .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV };
    struct addrinfo *a;
    int rc = getaddrinfo(ip, port, &hints, &a);
    if (rc) { dprintf(2, "%s\n", gai_strerror(rc)); return -1; }
    int fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (fd >= 0) {
        int yes = 1;
        if (server) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        rc = server ? bind(fd, a->ai_addr, a->ai_addrlen) : connect(fd, a->ai_addr, a->ai_addrlen);
        if (rc < 0 || (server && listen(fd, 32) < 0)) { perror("TCP"); close(fd); fd = -1; }
    } else perror("socket");
    freeaddrinfo(a);
    return fd;
}
