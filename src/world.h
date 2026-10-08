// world.h - все состояние модели в одной структуре
#pragma once
#include "common.h"
#include "config.h"
#include "car.h"
#include "ferry.h"
#include "dispatcher.h"
#include "stats.h"
#include "log.h"

struct World {
    Config cfg;
    int now;                        // время в тактах
    Car *cars; int ncars, cars_cap; // все созданные машины
    Queue q[2];                     // очереди на берегах
    int next_arr[2], gen[2];        // когда приедет следующая, сколько создано
    Ferry ferry;
    Stats st;
    int log_fd, events;
    int delivered, total;           // перевезено, всего
    int stamp, failed;
    const char *stop;               // причина остановки
};

#define CAR(w, id) (&(w)->cars[(id) - 1])   // машина по номеру

