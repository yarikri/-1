// stats.h - статистика и проверки
#pragma once
#include "common.h"

typedef struct {
    int trips, empty_trips, refusals;
    int carried[2];                 // перевезено (по берегу отправления)
    long wait_sum[2];               // [0] обычные, [1] экстренные
    int wait_n[2];
} Stats;

void check_invariants(World *w);    // семафор доступа должен быть занят
void stats_print(World *w);

const char *stop_text(StopReason reason);
