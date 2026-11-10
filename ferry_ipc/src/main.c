// main.c - паромная переправа (вариант 28), процессы
// BUILD_KIND: 0 - неименованные, 1 - именованные, 2 - независимый запуск
#include "world.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t children[2];
static int start_actors(World *w) {
    for (int role = 0; role < 2; role++) {
        pid_t pid = fork();
        if (!pid) { actor_run(w, role); log_close(); ipc_close(false); _exit(0); }
        if (pid < 0) { perror("fork"); return -1; }
        children[role] = pid;
    }
    return 0;
}

// Будим процессы, дожидаемся машин и остальных участников.
static void stop_processes(World *w) {
    world_lock(w); w->quit = true; world_unlock(w);
    ipc_wake_all();
    if (BUILD_KIND != 2) {
        cars_reap(w, true);
        for (int i = 0; i < 2; i++) if (children[i] > 0)
            while (waitpid(children[i], NULL, 0) < 0 && errno == EINTR) {}
    } else for (;;) {
        world_lock(w);
        bool done = true;
        for (int i = 0; i < 3; i++) if (w->started[i] && !w->finished[i]) done = false;
        world_unlock(w);
        if (done) break;
        usleep(10000);
    }
}

int main(int argc, char **argv) {
    Config cfg;
    config_defaults(&cfg);
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            if (config_load_file(&cfg, argv[++i])) return 2;
        } else if (eq) {
            *eq = 0;
            if (config_set(&cfg, argv[i], eq + 1)) return 2;
        } else {
            dprintf(2, "Использование: %s [-c файл.cfg] [параметр=значение ...]\n", argv[0]);
            return 2;
        }
    }
    if (config_validate(&cfg)) return 2;
    if (!cfg.seed) cfg.seed = (int)time(NULL);
    signals_init();
    if (ipc_create(BUILD_KIND != 0, BUILD_KIND == 2, &cfg)) return 2;
    World *w = ipc_world();
    if (log_open(w, true)) { perror("log"); ipc_close(true); return 2; }
    w->total = cfg.cars[LEFT] == UNLIMITED || cfg.cars[RIGHT] == UNLIMITED
                   ? UNLIMITED : cfg.cars[LEFT] + cfg.cars[RIGHT];
    logr(w, "=== Паромная переправа (seed=%d) ===", cfg.seed);
    ferry_init(w);
    if (!w->total) w->stop = STOP_EMPTY;
    logr(w, "IPC готово");
    if (BUILD_KIND != 2 && !w->stop) {
        w->started[ROLE_CARS] = true;
        cars_init(w);
        if (start_actors(w)) { w->failed = 1; w->stop = STOP_PROCESS; }
    }
    for (;;) {
        if (BUILD_KIND != 2) cars_reap(w, false);
        world_lock(w);
        if (!w->stop && interrupted) w->stop = STOP_SIGNAL;
        if (!w->running && w->started[0] && w->started[1] && w->started[2]) {
            w->start_ms = now_ms();
            w->running = true;
        }
        if (w->running) {
            if (BUILD_KIND != 2 && !w->stop) arrivals_step(w);
            check_invariants(w);
        }
        int t = now_ticks(w);
        if (!w->stop && cfg.max_time && t >= cfg.max_time) w->stop = STOP_TIME;
        bool done = w->stop != STOP_NONE;
        if (done) w->end_t = t;
        world_unlock(w);
        if (done) break;
        usleep(cfg.tick_ms * 1000);
    }
    stop_processes(w);
    stats_print(w);
    int rc = w->failed ? 3 : 0;
    log_close();
    ipc_close(true);
    return rc;
}
