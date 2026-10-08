// main.c - паромная переправа (вариант 28)
// запуск: ./ferry [-c файл.cfg] [параметр=значение ...]
#include "world.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_interrupted = 0;
static void on_signal(int sig) { (void)sig; g_interrupted = 1; }

// паром и машины, которые стоят в очереди с самого начала
static void world_init(World *w) {
    const Config *g = &w->cfg;
    w->total = (g->cars[LEFT] == UNLIMITED || g->cars[RIGHT] == UNLIMITED)
                   ? UNLIMITED : g->cars[LEFT] + g->cars[RIGHT];
    ferry_init(w);
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < g->initial[b]; i++) car_spawn(w, b);
        w->next_arr[b] = g->arr_min + rand() % (g->arr_max - g->arr_min + 1);
    }
    if (w->total == 0) w->stop = "машин нет";
}

int main(int argc, char **argv) {
    World w;
    memset(&w, 0, sizeof w);
    config_defaults(&w.cfg);

    // аргументы: -c файл.cfg и параметры key=value (пишите -c первым)
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            if (config_load_file(&w.cfg, argv[++i])) return 2;
        } else if (eq) {
            *eq = 0;
            if (config_set(&w.cfg, argv[i], eq + 1)) return 2;
        } else {
            dprintf(STDERR_FILENO, "Использование: %s [-c файл.cfg] [параметр=значение ...]\n", argv[0]);
            return 2;
        }
    }
    if (config_validate(&w.cfg)) return 2;

    if (!w.cfg.seed) w.cfg.seed = (int)time(NULL);
    srand((unsigned)w.cfg.seed);
    log_open(&w);

    // по Ctrl+C только ставим флаг, потом выходим из цикла
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    logr(&w, "=== Паромная переправа (seed=%d) ===", w.cfg.seed);
    world_init(&w);

    // главный цикл, один проход = один такт
    while (!w.stop && !g_interrupted) {
        w.events = 0;
        w.now++;
        arrivals_step(&w);
        ferry_step(&w);
        check_invariants(&w);
        if (w.events) usleep(w.cfg.delay_ms * 1000);
        if (!w.stop && w.cfg.max_time && w.now >= w.cfg.max_time)
            w.stop = "вышло время (max_time)";
    }
    if (!w.stop) w.stop = "прервано пользователем (Ctrl+C)";

    stats_print(&w);
    log_close(&w);
    free(w.cars); free(w.q[LEFT].ids); free(w.q[RIGHT].ids); free(w.ferry.on);
    return w.failed ? 3 : 0;
}
