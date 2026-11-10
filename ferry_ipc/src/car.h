// car.h - машины (у каждой свой процесс) и очереди
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <semaphore.h>
#include "common.h"

typedef enum { LOC_QUEUE, LOC_FERRY, LOC_DONE } CarLoc;
typedef struct Car {
    int id, mass, size;
    bool emergency;
    int bank;
    CarLoc loc;
    int t_arrive, skipped, rides, stamp;
    pid_t pid;
    bool reaped;
    int next;                   // следующая машина в очереди или на борту
    sem_t go;
} Car;

// В общей памяти храним номера ячеек, а не адреса другого процесса.
typedef struct { int head, n; } Queue;
void car_arrive(World *w, int bank);
void car_process(World *w, int slot);
void cars_process(World *w);
void cars_init(World *w);
void cars_reap(World *w, bool wait_all);
void car_str(const Car *c, char *buf, size_t n);
int queue_at(World *w, const Queue *q, int idx);
void queue_insert(World *w, Queue *q, int idx, int slot);
void queue_remove(World *w, Queue *q, int idx);
void arrivals_step(World *w);
bool more_coming(const World *w, int bank);
