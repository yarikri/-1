// car.h - машины и очереди
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "common.h"

typedef enum { LOC_QUEUE, LOC_FERRY, LOC_DONE } CarLoc;

typedef struct {
    int id;
    int mass;           // кг
    int size;           // сколько мест занимает
    bool emergency;
    int bank;           // где появилась
    CarLoc loc;
    int t_arrive;
    int skipped;        // сколько раз пропущена (lookahead)
    int rides;          // сколько раз перевезена, должно быть не больше 1
    int stamp;          // для проверки инвариантов
} Car;

typedef struct { int *ids; int n, cap; } Queue;

int car_spawn(World *w, int bank);
void car_str(const Car *c, char *buf, size_t n);
void queue_remove(Queue *q, int idx);
void arrivals_step(World *w);
bool more_coming(const World *w, int bank);

