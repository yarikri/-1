// sync.c - POSIX shared memory, семафоры и время
#include "world.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

static World *world;
static sem_t *handles[SEM_COUNT];
static bool created[SEM_COUNT], named, owns_memory;
static char base[64];
static struct LocalCar { Car *car; sem_t *go; } *cars;
static int ncars;               // локальные отображения общей памяти машин
volatile sig_atomic_t interrupted;

long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}
int now_ticks(World *w) { return w->running ? (int)((now_ms() - w->start_ms) / w->cfg.tick_ms) : 0; }
static void on_signal(int sig) { (void)sig; interrupted = 1; }
void signals_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void object_name(char *buf, int car, int sem) {
    if (car >= 0) snprintf(buf, 96, "%s_c%d%s", base, car, sem ? "_go" : "");
    else snprintf(buf, 96, "%s_s%d", base, sem);
}
static void *map_memory(const char *name, size_t size, bool create) {
    int fd = shm_open(name, O_RDWR | (create ? O_CREAT | O_EXCL : 0), 0600);
    if (fd < 0) return NULL;
    if (create && ftruncate(fd, size) < 0) { close(fd); shm_unlink(name); return NULL; }
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { if (create) shm_unlink(name); return NULL; }
    return p;
}

// Каждая новая машина получает свою общую память, как раньше получала calloc.
static Car *car_map(int slot, bool create) {
    if (slot >= ncars) {
        int count = slot + 1;
        void *p = realloc(cars, count * sizeof *cars);
        if (!p) { perror("realloc"); _exit(2); }
        cars = p;
        memset(cars + ncars, 0, (count - ncars) * sizeof *cars);
        ncars = count;
    }
    if (!cars[slot].car) {
        char name[96]; object_name(name, slot, 0);
        Car *c = map_memory(name, sizeof(Car), create);
        if (!c) return NULL;
        if (create) { memset(c, 0, sizeof *c); c->id = slot + 1; }
        if (!named && create && sem_init(&c->go, 1, 0) < 0) {
            munmap(c, sizeof *c); shm_unlink(name); return NULL;
        }
        if (named) {
            object_name(name, slot, 1);
            cars[slot].go = sem_open(name, create ? O_CREAT | O_EXCL : 0, 0600, 0);
            if (cars[slot].go == SEM_FAILED) {
                cars[slot].go = NULL;
                munmap(c, sizeof *c);
                if (create) { object_name(name, slot, 0); shm_unlink(name); }
                return NULL;
            }
        } else cars[slot].go = &c->go;
        cars[slot].car = c;
    }
    return cars[slot].car;
}
Car *ipc_car_create(int slot) { return car_map(slot, true); }
Car *ipc_car(int slot) {
    Car *c = car_map(slot, false);
    if (!c) { perror("shared car"); _exit(2); }
    return c;
}
static sem_t *semaphore(int index) {
    if (index < S_CAR) return handles[index];
    int slot = index - S_CAR;
    ipc_car(slot);
    return cars[slot].go;
}
void world_lock(World *w) {
    (void)w;
    while (sem_wait(handles[S_LOCK]) < 0) {
        if (errno != EINTR) { perror("sem_wait"); _exit(2); }
    }
}
void world_unlock(World *w) { (void)w; post_sem(S_LOCK); }
void post_sem(int index) { sem_post(semaphore(index)); }
bool is_quit(World *w) {
    world_lock(w);
    if (interrupted && !w->stop) w->stop = STOP_SIGNAL;
    bool quit = w->quit || w->stop;
    world_unlock(w);
    return quit;
}
void sleep_ticks(World *w, int n) {
    for (int i = 0; i < n && !is_quit(w); i++) usleep(w->cfg.tick_ms * 1000);
}
bool wait_sem(World *w, int index) {
    while (!is_quit(w)) {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_nsec += 100000000L;
        t.tv_sec += t.tv_nsec / 1000000000L;
        t.tv_nsec %= 1000000000L;
        if (sem_timedwait(semaphore(index), &t) == 0) return !is_quit(w);
        if (errno != EINTR && errno != ETIMEDOUT) return false;
    }
    return false;
}

World *ipc_world(void) { return world; }
int ipc_create(bool use_named, bool independent, const Config *cfg) {
    named = use_named;
    snprintf(base, sizeof base, "/ferry_%lu_%ld", (unsigned long)getuid(), independent ? 0L : (long)getpid());
    world = map_memory(base, sizeof(World), true);
    if (!world) { perror("shm_open/mmap"); return -1; }
    owns_memory = true;
    memset(world, 0, sizeof *world);
    world->named = named;
    world->cfg = *cfg;
    for (int i = 0; i < SEM_COUNT; i++) {
        if (named) {
            char name[96]; object_name(name, -1, i);
            handles[i] = sem_open(name, O_CREAT | O_EXCL, 0600, i == S_LOCK ? 1 : 0);
            if (handles[i] == SEM_FAILED) handles[i] = NULL;
        } else {
            handles[i] = &world->sems[i];
            if (sem_init(handles[i], 1, i == S_LOCK ? 1 : 0) < 0) handles[i] = NULL;
        }
        if (!handles[i]) { perror("semaphore"); ipc_close(true); return -1; }
        created[i] = true;
    }
    return 0;
}
int ipc_attach(void) {
    named = true;
    snprintf(base, sizeof base, "/ferry_%lu_0", (unsigned long)getuid());
    world = map_memory(base, sizeof(World), false);
    if (!world) { perror("shm_open: сначала запустите ferry_independent"); return -1; }
    for (int i = 0; i < SEM_COUNT; i++) {
        char name[96]; object_name(name, -1, i);
        handles[i] = sem_open(name, 0);
        if (handles[i] == SEM_FAILED) { handles[i] = NULL; ipc_close(false); return -1; }
    }
    return 0;
}
void ipc_wake_all(void) {
    for (int i = 1; i < SEM_COUNT; i++) post_sem(i);
    for (int i = 0; i < world->nall; i++) post_sem(S_CAR + i);
}
void ipc_close(bool owner) {
    int count = world ? world->nall : 0;
    for (int i = 0; i < count; i++) {
        char name[96];
        if (owner && named) { object_name(name, i, 1); sem_unlink(name); }
        if (owner) { object_name(name, i, 0); shm_unlink(name); }
    }
    for (int i = 0; i < ncars; i++) if (cars[i].car) {
        if (named) sem_close(cars[i].go);
        else if (owner) sem_destroy(cars[i].go);
        munmap(cars[i].car, sizeof(Car));
    }
    free(cars); cars = NULL; ncars = 0;
    for (int i = 0; i < SEM_COUNT; i++) if (handles[i]) {
        if (named) sem_close(handles[i]);
        else if (owner && created[i]) sem_destroy(handles[i]);
        if (owner && named && created[i]) {
            char name[96]; object_name(name, -1, i); sem_unlink(name);
        }
        handles[i] = NULL;
    }
    if (world) { munmap(world, sizeof(World)); world = NULL; }
    if (owner && owns_memory) shm_unlink(base);
}
void actor_run(World *w, int role) {
    world_lock(w); w->started[role] = true; world_unlock(w);
    while (!is_quit(w)) {
        world_lock(w); bool running = w->running; world_unlock(w);
        if (running) break;
        usleep(10000);
    }
    if (!is_quit(w)) {
        if (role == ROLE_FERRY) ferry_process(w);
        else if (role == ROLE_DISPATCHER) dispatcher_process(w);
        else cars_process(w);
    }
    world_lock(w); w->finished[role] = true; world_unlock(w);
}
