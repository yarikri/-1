// net.c - poll обслуживает берега и наблюдателей между тактами модели
#include "world.h"
#include "net.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define CLIENTS 64
static struct Peer {
    int fd, role;
    int pending;                 // 0 - нет запроса, 1 - ждем, 2 - ответ получен
    size_t n;
    char out[8192];
} peers[CLIENTS];
static int listener;
static World *model;

static void drop(struct Peer *p) { close(p->fd); p->fd = -1; }
static struct Peer *shore(int bank) {
    for (int i = 0; i < CLIENTS; i++)
        if (peers[i].fd >= 0 && peers[i].role == bank) return &peers[i];
    return NULL;
}
static void queue(struct Peer *p, const char *s) {
    size_t n = strlen(s);
    if (n > sizeof p->out - p->n) { drop(p); return; }
    memcpy(p->out + p->n, s, n);
    p->n += n;
}
int net_connected(int role) { return shore(role) != NULL; }

void net_request(int bank) {
    struct Peer *p = shore(bank);
    if (p && !p->pending) { p->pending = 1; queue(p, "?\n"); }
}
int net_take(int bank) {
    struct Peer *p = shore(bank);
    if (!p || p->pending != 2) return 0;
    p->pending = 0;
    return 1;
}
void net_event(World *w, int role, const char *line) {
    model = w;
    for (int i = 0; i < CLIENTS; i++) {
        struct Peer *p = &peers[i];
        if (p->fd >= 0 && p->role >= 0 &&
            (p->role == OBSERVER || role == p->role)) queue(p, line);
    }
}

int net_listen(const char *ip, const char *port) {
    for (int i = 0; i < CLIENTS; i++) peers[i].fd = -1;
    listener = tcp_open(ip, port, 1);
    return listener;
}
static void receive(struct Peer *p) {
    char buf[128];
    ssize_t n = recv(p->fd, buf, sizeof buf, 0);
    if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (n <= 0) { drop(p); return; }
    for (ssize_t i = 0; i < n; i++) {
        char c = buf[i];
        if (p->role < 0 && (c == 'L' || c == 'R' || c == 'O')) {
            int role = c == 'L' ? SHORE_LEFT : c == 'R' ? SHORE_RIGHT : OBSERVER;
            if (role != OBSERVER && shore(role)) { drop(p); return; }
            p->role = role;
            queue(p, "Подключение принято\n");
            if (role == OBSERVER && model) {
                char st[200], line[400];
                static const char *const state[] = {"погрузка", "закрытие аппарели", "в пути", "выгрузка"};
                ferry_status(model, st, sizeof st);
                snprintf(line, sizeof line, "[t=%04d] Состояние: %s, берег: %s, очереди: %d и %d, рейсов: %d, перевезено: %d. Паром %s\n",
                         model->now, state[model->ferry.state], BANK[model->ferry.bank],
                         model->q[0].n, model->q[1].n, model->ferry.trips, model->delivered, st);
                queue(p, line);
            }
        } else if (c == 'A' && p->role >= 0 && p->role < OBSERVER && p->pending == 1)
            p->pending = 2;
        else { drop(p); return; }
    }
}

void net_poll(int ms) {
    struct pollfd f[CLIENTS + 1] = {{ listener, POLLIN, 0 }};
    for (int i = 0; i < CLIENTS; i++)
        f[i + 1] = (struct pollfd){ peers[i].fd, POLLIN | (peers[i].n ? POLLOUT : 0), 0 };
    if (poll(f, CLIENTS + 1, ms) <= 0) return;
    if (f[0].revents & POLLIN) {
        int fd = accept4(listener, NULL, NULL, SOCK_NONBLOCK);
        if (fd >= 0) {
            int yes = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
            int i = 0;
            while (i < CLIENTS && peers[i].fd >= 0) i++;
            if (i == CLIENTS) close(fd);
            else { peers[i].fd = fd; peers[i].role = -1; peers[i].pending = 0; peers[i].n = 0; }
        }
    }
    for (int i = 0; i < CLIENTS; i++) {
        struct Peer *p = &peers[i];
        int events = f[i + 1].revents;
        if (p->fd < 0 || !events) continue;
        if (events & POLLIN) receive(p);
        if (p->fd < 0) continue;
        if (events & (POLLHUP | POLLERR | POLLNVAL)) { drop(p); continue; }
        if ((events & POLLOUT) && p->n) {
            ssize_t n = send(p->fd, p->out, p->n, MSG_NOSIGNAL);
            if (n > 0) { p->n -= n; memmove(p->out, p->out + n, p->n); }
            else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) drop(p);
        }
    }
}
void net_close(void) {
    close(listener);
    listener = -1;
    // Даем отправить итоговые строки, затем клиенты получают EOF.
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        int pending = 0;
        for (int i = 0; i < CLIENTS; i++) pending |= peers[i].fd >= 0 && peers[i].n > 0;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (!pending || (now.tv_sec - start.tv_sec)*1000 + (now.tv_nsec - start.tv_nsec)/1000000 >= 500) break;
        net_poll(20);
    }
    for (int i = 0; i < CLIENTS; i++) if (peers[i].fd >= 0) drop(&peers[i]);
}
