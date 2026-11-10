// world.h - все общее состояние модели в POSIX shared memory
#pragma once
#include "common.h"
#include "config.h"
#include "car.h"
#include "ferry.h"
#include "dispatcher.h"
#include "stats.h"
#include "log.h"
#include "sync.h"

struct World {
    bool named;
    Config cfg;
    long start_ms;
    int nall;
    Queue q[2];
    int next_arr[2], gen[2];
    Ferry ferry;
    Stats st;
    int delivered, total;
    int end_t, stamp, failed;
    StopReason stop;             // строковые указатели между процессами не передаем
    bool quit, running;
    bool started[3];
    bool finished[3];
    sem_t sems[SEM_COUNT];       // используются только в неименованном варианте
};
#define CAR(w, slot) ((void)(w), ipc_car(slot))
