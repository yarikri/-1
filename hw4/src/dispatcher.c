// dispatcher.c - диспетчер решает: ждать, грузить машину или отплывать
// стратегии:
//   fifo      - по очереди, не влезла первая - отплываем
//   lookahead - можно взять другую машину из первых window
#include "world.h"

Decision dispatcher_decide(World *w) {
    const Config *g = &w->cfg;
    const Ferry *f = &w->ferry;
    int b = f->bank, o = OTHER(b);
    const Queue *q = &w->q[b];
    Decision d = { D_WAIT, -1, 0 };
#define DEPART(r) do { d.act = D_DEPART; d.reason = (r); return d; } while (0)

    // паром полный
    if (f->n > 0 && (f->n >= g->max_cars || f->slots >= g->slots || f->mass >= g->capacity_kg))
        DEPART(DEP_FULL);
    // вышло время ожидания
    if (f->n > 0 && f->wait >= g->max_wait) DEPART(DEP_TIMEOUT);

    // очередь пустая
    if (q->n == 0) {
        if (f->n > 0) {
            if (!more_coming(w, b)) DEPART(DEP_NO_MORE);
            return d;
        }
        // паром пустой, плывем на другой берег, если там ждут
        bool here_over = !more_coming(w, b);
        if ((w->q[o].n > 0 && (here_over || f->wait >= g->max_wait)) ||
            (here_over && more_coming(w, o)))
            DEPART(DEP_EMPTY);
        return d;
    }

    // пробуем взять первую машину
    int why, head = q->ids[0];
    const Car *h = CAR(w, head);
    if (ferry_fits(w, h, &why)) { d.act = D_LOAD; d.car = head; return d; }

    char s[96];
    car_str(h, s, sizeof s);
    logw(w, "Отказ в погрузке: %s, причина: %s", s, REFUSE_TXT[why]);
    w->st.refusals++;

    // lookahead: ищем другую машину
    if (g->strategy == 1 && h->skipped < g->skip_limit)
        for (int i = 1; i < q->n && i <= g->window; i++) {
            const Car *c = CAR(w, q->ids[i]);
            int why2;
            if (!ferry_fits(w, c, &why2)) continue;
            CAR(w, head)->skipped++;
            car_str(c, s, sizeof s);
            logw(w, "Диспетчер пропустил #%d и грузит %s", head, s);
            d.act = D_LOAD; d.car = c->id;
            return d;
        }
    DEPART(DEP_BLOCKED);
}
