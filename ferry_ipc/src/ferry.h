// ferry.h - паром (отдельный процесс)
#pragma once
#include "car.h"
typedef enum { F_LOADING, F_RAMP_CLOSING, F_CROSSING, F_UNLOADING } FerryState;
enum { REFUSE_MASS, REFUSE_SLOTS, REFUSE_COUNT };
typedef struct {
    FerryState state;
    int bank;
    Queue on;
    int n, mass, slots;
    int cur_car;                // номер ячейки машины или NO_CAR
    int t_open;
    bool ramp_open;
    int trips;
} Ferry;
void ferry_init(World *w);
void ferry_process(World *w);
void ferry_board(World *w, Car *c);
void ferry_close(World *w, int reason);
bool ferry_fits(const World *w, const Car *c, int *why);
void ferry_status(const World *w, char *buf, size_t n);
