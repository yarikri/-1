// ferry.c - паром
// состояния: погрузка -> закрытие аппарели -> рейс -> выгрузка -> ...
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ferry_status(const World *w, char *buf, size_t n) {
    const Ferry *f = &w->ferry;
    int len = snprintf(buf, n, "[%d/%d авто, %d/%d мест, %d/%d кг]", f->n, w->cfg.max_cars,
                       f->slots, w->cfg.slots, f->mass, w->cfg.capacity_kg);
    for (int i = 0; i < f->n && len < (int)n - 12; i++)
        len += snprintf(buf + len, n - len, " #%d", f->on[i]);
}

// влезет ли машина, иначе в why причина
bool ferry_fits(const World *w, const Car *c, int *why) {
    const Ferry *f = &w->ferry;
    if (f->n >= w->cfg.max_cars)                *why = REFUSE_COUNT;
    else if (f->slots + c->size > w->cfg.slots) *why = REFUSE_SLOTS;
    else if (f->mass + c->mass > w->cfg.capacity_kg) *why = REFUSE_MASS;
    else return true;
    return false;
}

void ferry_init(World *w) {
    Ferry *f = &w->ferry;
    memset(f, 0, sizeof *f);
    f->on = calloc(w->cfg.max_cars, sizeof(int));
    f->state = F_LOADING;
    f->bank = LEFT;
    f->cur_car = -1;
    f->ramp_open = true;
    logw(w, "Паром у берега: %s, начинаем погрузку", BANK[f->bank]);
}

static void open_for_loading(World *w) {
    Ferry *f = &w->ferry;
    f->state = F_LOADING;
    f->wait = 0;
    f->cur_car = -1;
    f->ramp_open = true;
    logw(w, "Погрузка, берег: %s (в очереди: %d)", BANK[f->bank], w->q[f->bank].n);
    if (w->total != UNLIMITED && w->delivered >= w->total)
        w->stop = "все автомобили перевезены";
    else if (w->cfg.max_trips && f->trips >= w->cfg.max_trips)
        w->stop = "выполнено заданное число рейсов";
}

// машина заезжает на паром (место занимаем сразу)
static void board(World *w, int id) {
    Ferry *f = &w->ferry;
    Queue *q = &w->q[f->bank];
    Car *c = CAR(w, id);
    for (int i = 0; i < q->n; i++)
        if (q->ids[i] == id) { queue_remove(q, i); break; }
    c->loc = LOC_FERRY;
    c->rides++;
    f->on[f->n++] = id;
    f->mass += c->mass;
    f->slots += c->size;
    f->cur_car = id;
    f->timer = w->cfg.load_time;
    int wt = w->now - c->t_arrive;
    w->st.wait_sum[c->emergency] += wt;
    w->st.wait_n[c->emergency]++;
    char s[96];
    car_str(c, s, sizeof s);
    logw(w, "Грузим %s (ждала %d мин)", s, wt);
}

static void step_loading(World *w) {
    Ferry *f = &w->ferry;
    char s[96], st[200];
    f->wait++;
    if (f->cur_car >= 0) {
        if (--f->timer <= 0) {
            car_str(CAR(w, f->cur_car), s, sizeof s);
            ferry_status(w, st, sizeof st);
            logw(w, "Погрузили %s. Паром %s", s, st);
            f->cur_car = -1;
        }
        return;
    }
    Decision d = dispatcher_decide(w);
    if (d.act == D_LOAD) board(w, d.car);
    else if (d.act == D_DEPART) {
        ferry_status(w, st, sizeof st);
        logw(w, "Конец погрузки (%s). Паром %s", DEP_TXT[d.reason], st);
        f->state = F_RAMP_CLOSING;
        f->timer = w->cfg.ramp_time;
    }
}

static void depart(World *w) {
    Ferry *f = &w->ferry;
    f->ramp_open = false;
    f->state = F_CROSSING;
    f->timer = w->cfg.cross_time;
    f->trips++;
    w->st.trips++;
    if (!f->n) w->st.empty_trips++;
    logw(w, "Отправление, рейс %d, берег отправления: %s", f->trips, BANK[f->bank]);
}

static void arrive(World *w) {
    Ferry *f = &w->ferry;
    f->bank = OTHER(f->bank);
    f->ramp_open = true;
    f->state = F_UNLOADING;
    f->cur_car = -1;
    logw(w, "Паром приплыл, берег: %s, на борту %d авто", BANK[f->bank], f->n);
    if (!f->n) open_for_loading(w);
}

// выгружаем по одной машине
static void step_unloading(World *w) {
    Ferry *f = &w->ferry;
    char s[96], st[200];
    if (f->cur_car < 0) {
        f->cur_car = f->on[0];
        f->timer = w->cfg.unload_time;
        car_str(CAR(w, f->cur_car), s, sizeof s);
        logw(w, "Выгружаем %s", s);
        return;
    }
    if (--f->timer > 0) return;
    Car *c = CAR(w, f->cur_car);
    memmove(f->on, f->on + 1, (--f->n) * sizeof(int));
    f->mass -= c->mass;
    f->slots -= c->size;
    c->loc = LOC_DONE;
    w->st.carried[c->bank]++;
    w->delivered++;
    car_str(c, s, sizeof s);
    ferry_status(w, st, sizeof st);
    logw(w, "Выгрузили %s. Паром %s", s, st);
    f->cur_car = -1;
    if (!f->n) open_for_loading(w);
}

void ferry_step(World *w) {
    Ferry *f = &w->ferry;
    switch (f->state) {
    case F_LOADING:      step_loading(w); break;
    case F_RAMP_CLOSING: if (--f->timer <= 0) depart(w); break;
    case F_CROSSING:     if (--f->timer <= 0) arrive(w); break;
    case F_UNLOADING:    step_unloading(w); break;
    }
}
