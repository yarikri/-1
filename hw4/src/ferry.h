// ferry.h - паром
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "car.h"

typedef enum { F_LOADING, F_RAMP_CLOSING, F_CROSSING, F_UNLOADING } FerryState;
enum { REFUSE_MASS, REFUSE_SLOTS, REFUSE_COUNT };   // причины отказа

typedef struct {
    FerryState state;
    int bank;           // у какого берега стоит (в рейсе - откуда отплыл)
    int *on;            // машины на борту
    int n, mass, slots; // сколько занято
    int cur_car;        // кого грузим/выгружаем (-1 - никого)
    int timer;          // сколько тактов осталось до конца операции
    int wait;           // сколько стоим у берега
    bool ramp_open;
    int trips;
} Ferry;

void ferry_init(World *w);
void ferry_step(World *w);
bool ferry_fits(const World *w, const Car *c, int *why);
void ferry_status(const World *w, char *buf, size_t n);

