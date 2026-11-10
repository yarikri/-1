// car.c - машины и очереди
// у каждой машины свой процесс, она ждет на своем семафоре go
// экстренные встают вперед, но не обгоняют тех, кто ждет дольше aging_limit
#include "world.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int rnd(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

void car_str(const Car *c, char *buf, size_t n) {
    const char *kind = c->emergency ? "экстренная" : c->size > 1 ? "большая" : "легковая";
    snprintf(buf, n, "#%d (%s, %d кг, %d мест)", c->id, kind, c->mass, c->size);
}

int queue_at(World *w, const Queue *q, int idx) {
    int slot = q->head;
    for (int i = 0; i < idx; i++) slot = CAR(w, slot)->next;
    return slot;
}
void queue_insert(World *w, Queue *q, int idx, int slot) {
    int *link = idx ? &CAR(w, queue_at(w, q, idx - 1))->next : &q->head;
    CAR(w, slot)->next = q->n > idx ? *link : NO_CAR;
    *link = slot;
    q->n++;
}
void queue_remove(World *w, Queue *q, int idx) {
    int *link = idx ? &CAR(w, queue_at(w, q, idx - 1))->next : &q->head;
    *link = CAR(w, *link)->next;
    q->n--;
}
static void queue_arrive(World *w, int bank, Car *car) {
    Queue *q = &w->q[bank];
    int pos = q->n;
    if (car->emergency && w->cfg.priority) {
        pos = 0;
        while (pos < q->n) {
            const Car *o = CAR(w, queue_at(w, q, pos));
            if (!o->emergency && now_ticks(w) - o->t_arrive < w->cfg.aging_limit) break;
            pos++;
        }
    }
    queue_insert(w, q, pos, car->id - 1);
}

// поведение машины то же: ждет заезда, заезжает, ждет выгрузки, съезжает
void car_process(World *w, int slot) {
    Car *c = CAR(w, slot);
    Ferry *f = &w->ferry;
    char s[96], st[200];
    if (!wait_sem(w, S_CAR + slot)) return;
    sleep_ticks(w, w->cfg.load_time);
    if (is_quit(w)) return;
    world_lock(w);
    car_str(c, s, sizeof s);
    ferry_status(w, st, sizeof st);
    logw(w, "Погрузили %s. Паром %s", s, st);
    f->cur_car = NO_CAR;
    world_unlock(w);

    if (!wait_sem(w, S_CAR + slot)) return;
    sleep_ticks(w, w->cfg.unload_time);
    if (is_quit(w)) return;
    world_lock(w);
    queue_remove(w, &f->on, 0);
    f->n--;
    f->mass -= c->mass;
    f->slots -= c->size;
    c->loc = LOC_DONE;
    w->st.carried[c->bank]++;
    w->delivered++;
    ferry_status(w, st, sizeof st);
    logw(w, "Выгрузили %s. Паром %s", s, st);
    f->cur_car = NO_CAR;
    world_unlock(w);
    post_sem(S_UNLOADED);
}

// Вызывается при занятом семафоре доступа. Дочерний процесс дождется его освобождения.
void car_arrive(World *w, int bank) {
    int slot = w->nall;
    Car *c = ipc_car_create(slot);
    if (!c) { perror("shared car"); w->failed = 1; w->stop = STOP_PROCESS; return; }
    w->nall++;
    int r = rand() % 100;
    c->size = r < 60 ? 1 : r < 80 ? 2 : r < 90 ? 3 : 4;
    if (c->size > w->cfg.size_max) c->size = w->cfg.size_max;
    c->mass = c->size * rnd(w->cfg.mass_min, w->cfg.mass_max);
    c->emergency = rand() % 100 < w->cfg.emergency_pct;
    c->bank = bank;
    c->t_arrive = now_ticks(w);
    w->gen[bank]++;
    queue_arrive(w, bank, c);
    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Приехала машина %s на %s берег (в очереди: %d)", s, BANK[bank], w->q[bank].n);
    pid_t pid = fork();
    if (pid == 0) {
        car_process(w, slot);
        log_close();
        ipc_close(false);
        _exit(0);
    }
    if (pid < 0) {
        perror("fork(car)");
        c->reaped = true;
        w->failed = 1;
        w->stop = STOP_PROCESS;
    } else c->pid = pid;
}

bool more_coming(const World *w, int bank) {
    return w->cfg.cars[bank] == UNLIMITED || w->gen[bank] < w->cfg.cars[bank];
}

void arrivals_step(World *w) {
    for (int b = 0; b < 2 && !w->stop; b++)
        if (more_coming(w, b) && w->next_arr[b] <= now_ticks(w)) {
            car_arrive(w, b);
            w->next_arr[b] = now_ticks(w) + rnd(w->cfg.arr_min, w->cfg.arr_max);
        }
}

// Только создатель машин вызывает waitpid: чужой процесс их ждать не может.
void cars_reap(World *w, bool wait_all) {
    for (;;) {
        int active = 0;
        world_lock(w);
        for (int i = 0; i < w->nall; i++) {
            Car *c = CAR(w, i);
            if (c->pid <= 0 || c->reaped) continue;
            pid_t rc = waitpid(c->pid, NULL, WNOHANG);
            if (rc == c->pid || (rc < 0 && errno == ECHILD)) c->reaped = true;
            else active++;
        }
        world_unlock(w);
        if (!wait_all || !active) break;
        usleep(10000);
    }
}

void cars_init(World *w) {
    srand((unsigned)w->cfg.seed);
    world_lock(w);
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < w->cfg.initial[b] && !w->stop; i++) car_arrive(w, b);
        w->next_arr[b] = rnd(w->cfg.arr_min, w->cfg.arr_max);
    }
    world_unlock(w);
}

// При независимом запуске прежняя часть main вынесена в отдельную программу.
void cars_process(World *w) {
    cars_init(w);
    while (!is_quit(w)) {
        cars_reap(w, false);
        world_lock(w);
        if (!w->stop) arrivals_step(w);
        world_unlock(w);
        sleep_ticks(w, 1);
    }
    cars_reap(w, true);
}
