// stats.c - проверки и итоги
#include "world.h"
#include <stdio.h>

static void fail(World *w, const char *msg) {
    logw(w, "НАРУШЕНИЕ ИНВАРИАНТА: %s", msg);
    w->failed = 1;
    w->stop = "нарушен инвариант";
}

// проверка инвариантов после каждого такта
void check_invariants(World *w) {
    const Ferry *f = &w->ferry;
    int mass = 0, slots = 0;
    w->stamp++;
    for (int i = 0; i < f->n; i++) {
        Car *c = CAR(w, f->on[i]);
        if (c->loc != LOC_FERRY || c->stamp == w->stamp) fail(w, "машина в двух местах сразу");
        c->stamp = w->stamp;
        if (c->rides != 1) fail(w, "машину перевозят не один раз");
        // при выгрузке паром уже у другого берега
        if (c->bank != (f->state == F_UNLOADING ? OTHER(f->bank) : f->bank))
            fail(w, "погрузка или выгрузка не у своего берега");
        mass += c->mass;
        slots += c->size;
    }
    if (mass != f->mass || slots != f->slots) fail(w, "неверно посчитано заполнение");
    if (f->mass > w->cfg.capacity_kg || f->slots > w->cfg.slots || f->n > w->cfg.max_cars)
        fail(w, "превышена грузоподъемность или вместимость");
    if (f->state == F_CROSSING && (f->ramp_open || f->cur_car >= 0))
        fail(w, "паром плывет с открытой аппарелью или идет погрузка");
    for (int b = 0; b < 2; b++)
        for (int i = 0; i < w->q[b].n; i++) {
            Car *c = CAR(w, w->q[b].ids[i]);
            if (c->loc != LOC_QUEUE || c->bank != b || c->rides || c->stamp == w->stamp)
                fail(w, "машина в двух местах сразу или ошибка в очереди");
            c->stamp = w->stamp;
        }
}

static double avg(long sum, int n) { return n ? (double)sum / n : 0.0; }

void stats_print(World *w) {
    const Stats *s = &w->st;
    int not_come = 0;                   // машины, которые еще не приехали
    for (int b = 0; b < 2; b++)
        if (w->cfg.cars[b] != UNLIMITED) not_come += w->cfg.cars[b] - w->gen[b];
    int left = w->q[LEFT].n + w->q[RIGHT].n + w->ferry.n + not_come;

    logr(w, "\nИтоги:");
    logr(w, "Причина завершения: %s", w->stop);
    logr(w, "Время: %d мин", w->now);
    logr(w, "Рейсов: %d (пустых: %d)", s->trips, s->empty_trips);
    logr(w, "Отказов в погрузке: %d", s->refusals);
    logr(w, "Перевезено машин: %d (слева %d, справа %d)", w->delivered,
         s->carried[LEFT], s->carried[RIGHT]);
    logr(w, "Не перевезено: %d (в очередях %d+%d, на пароме %d, не приехали %d)",
         left, w->q[LEFT].n, w->q[RIGHT].n, w->ferry.n, not_come);
    logr(w, "Среднее ожидание: обычные %.1f мин, экстренные %.1f мин",
         avg(s->wait_sum[0], s->wait_n[0]), avg(s->wait_sum[1], s->wait_n[1]));
}
