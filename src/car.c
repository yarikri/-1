// car.c - машины и очереди
// экстренные встают вперед, но не обгоняют тех, кто ждет дольше aging_limit
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rnd(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

void car_str(const Car *c, char *buf, size_t n) {
    const char *kind = c->emergency ? "экстренная" : c->size > 1 ? "большая" : "легковая";
    snprintf(buf, n, "#%d (%s, %d кг, %d мест)", c->id, kind, c->mass, c->size);
}

void queue_remove(Queue *q, int idx) {
    memmove(&q->ids[idx], &q->ids[idx + 1], (q->n - idx - 1) * sizeof(int));
    q->n--;
}

static void queue_insert(World *w, int bank, int id) {
    Queue *q = &w->q[bank];
    if (q->n == q->cap) {
        q->cap = q->cap ? q->cap * 2 : 16;
        q->ids = realloc(q->ids, q->cap * sizeof(int));
    }
    int pos = q->n;
    if (CAR(w, id)->emergency && w->cfg.priority) {
        pos = 0;    // пропускаем экстренных и тех, кто давно ждет
        while (pos < q->n) {
            const Car *o = CAR(w, q->ids[pos]);
            if (!o->emergency && w->now - o->t_arrive < w->cfg.aging_limit) break;
            pos++;
        }
    }
    memmove(&q->ids[pos + 1], &q->ids[pos], (q->n - pos) * sizeof(int));
    q->ids[pos] = id;
    q->n++;
}

// новая машина со случайными параметрами
int car_spawn(World *w, int bank) {
    if (w->ncars == w->cars_cap) {
        w->cars_cap = w->cars_cap ? w->cars_cap * 2 : 64;
        w->cars = realloc(w->cars, w->cars_cap * sizeof(Car));
    }
    Car *c = &w->cars[w->ncars];
    memset(c, 0, sizeof *c);
    c->id = ++w->ncars;
    int r = rand() % 100;
    c->size = r < 60 ? 1 : r < 80 ? 2 : r < 90 ? 3 : 4;
    if (c->size > w->cfg.size_max) c->size = w->cfg.size_max;
    c->mass = c->size * rnd(w->cfg.mass_min, w->cfg.mass_max);
    c->emergency = rand() % 100 < w->cfg.emergency_pct;
    c->bank = bank;
    c->t_arrive = w->now;
    w->gen[bank]++;
    queue_insert(w, bank, c->id);

    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Приехала машина %s на %s берег (в очереди: %d)",
         s, BANK[bank], w->q[bank].n);
    return c->id;
}

bool more_coming(const World *w, int bank) {
    return w->cfg.cars[bank] == UNLIMITED || w->gen[bank] < w->cfg.cars[bank];
}

void arrivals_step(World *w) {
    for (int b = 0; b < 2; b++)
        if (more_coming(w, b) && w->next_arr[b] <= w->now) {
            car_spawn(w, b);
            w->next_arr[b] = w->now + rnd(w->cfg.arr_min, w->cfg.arr_max);
        }
}
