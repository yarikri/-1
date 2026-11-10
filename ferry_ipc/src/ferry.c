// ferry.c - паром (свой процесс)
// состояния: погрузка -> закрытие аппарели -> рейс -> выгрузка -> ...
// все изменения общих данных делаем под семафором доступа S_LOCK
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ferry_status(const World *w, char *buf, size_t n) {
    const Ferry *f = &w->ferry;
    int len = snprintf(buf, n, "[%d/%d авто, %d/%d мест, %d/%d кг]", f->n, w->cfg.max_cars,
                       f->slots, w->cfg.slots, f->mass, w->cfg.capacity_kg);
    for (int i = 0; i < f->n && len < (int)n - 12; i++)
        len += snprintf(buf + len, n - len, " #%d", CAR(w, queue_at((World *)w, &f->on, i))->id);
}

// влезет ли машина, иначе в why причина
bool ferry_fits(const World *w, const Car *c, int *why) {
    const Ferry *f = &w->ferry;
    if (f->n >= w->cfg.max_cars)                     *why = REFUSE_COUNT;
    else if (f->slots + c->size > w->cfg.slots)      *why = REFUSE_SLOTS;
    else if ((long)f->mass + c->mass > w->cfg.capacity_kg) *why = REFUSE_MASS;
    else return true;
    return false;
}

void ferry_init(World *w) {
    Ferry *f = &w->ferry;
    memset(f, 0, sizeof *f);
    f->cur_car = NO_CAR;
    f->state = F_LOADING;
    f->bank = LEFT;
    f->ramp_open = true;
    logw(w, "Паром у берега: %s, начинаем погрузку", BANK[f->bank]);
}

static void open_for_loading(World *w) {
    Ferry *f = &w->ferry;
    f->state = F_LOADING;
    f->t_open = now_ticks(w);
    f->cur_car = NO_CAR;
    f->ramp_open = true;
    logw(w, "Погрузка, берег: %s (в очереди: %d)", BANK[f->bank], w->q[f->bank].n);
    if (w->total != UNLIMITED && w->delivered >= w->total)
        w->stop = STOP_ALL;
    else if (w->cfg.max_trips && f->trips >= w->cfg.max_trips)
        w->stop = STOP_TRIPS;
}

// диспетчер пустил машину: она уходит из очереди на паром, место занимаем сразу
void ferry_board(World *w, Car *c) {
    Ferry *f = &w->ferry;
    Queue *q = &w->q[f->bank];
    for (int i = 0; i < q->n; i++)
        if (CAR(w, queue_at(w, q, i)) == c) { queue_remove(w, q, i); break; }
    c->loc = LOC_FERRY;
    c->rides++;
    queue_insert(w, &f->on, f->n++, c->id - 1);
    f->mass += c->mass;
    f->slots += c->size;
    f->cur_car = c->id - 1;
    int wt = now_ticks(w) - c->t_arrive;
    w->st.wait_sum[c->emergency] += wt;
    w->st.wait_n[c->emergency]++;
    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Грузим %s (ждала %d мин)", s, wt);
    post_sem(S_CAR + c->id - 1);                   // будим процесс машины
}

// диспетчер закончил погрузку: будим паром
void ferry_close(World *w, int reason) {
    Ferry *f = &w->ferry;
    char st[200];
    ferry_status(w, st, sizeof st);
    logw(w, "Конец погрузки (%s). Паром %s", DEP_TXT[reason], st);
    f->state = F_RAMP_CLOSING;
    post_sem(S_DEPART);
}

void ferry_process(World *w) {
    Ferry *f = &w->ferry;
    char s[96];
    for (;;) {
        if (!wait_sem(w, S_DEPART)) break;               // ждем, пока диспетчер разрешит отплыть
        if (is_quit(w)) break;
        sleep_ticks(w, w->cfg.ramp_time);   // закрываем аппарель
        if (is_quit(w)) break;

        world_lock(w);
        f->ramp_open = false;
        f->state = F_CROSSING;
        f->trips++;
        w->st.trips++;
        if (!f->n) w->st.empty_trips++;
        logw(w, "Отправление, рейс %d, берег отправления: %s", f->trips, BANK[f->bank]);
        world_unlock(w);
        sleep_ticks(w, w->cfg.cross_time);  // плывем
        if (is_quit(w)) break;

        world_lock(w);
        f->bank = OTHER(f->bank);
        f->ramp_open = true;
        f->state = F_UNLOADING;
        logw(w, "Паром приплыл, берег: %s, на борту %d авто", BANK[f->bank], f->n);
        world_unlock(w);

        // выгружаем по одной машине: будим ее и ждем, пока съедет
        for (;;) {
            world_lock(w);
            Car *c = f->n ? CAR(w, f->on.head) : NULL;
            if (c) {
                f->cur_car = c->id - 1;
                car_str(c, s, sizeof s);
                logw(w, "Выгружаем %s", s);
            }
            world_unlock(w);
            if (!c) break;
            post_sem(S_CAR + c->id - 1);
            if (!wait_sem(w, S_UNLOADED)) return;
            if (is_quit(w)) return;
        }

        world_lock(w);
        open_for_loading(w);
        world_unlock(w);
    }
    return;
}
