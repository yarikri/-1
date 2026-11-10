// common.h - общие константы
#pragma once
#define LEFT 0
#define RIGHT 1
#define OTHER(b) (1 - (b))
#define UNLIMITED (-1)
#define NO_CAR (-1)
typedef struct World World;

typedef enum {
    STOP_NONE, STOP_ALL, STOP_EMPTY, STOP_TRIPS, STOP_TIME, STOP_SIGNAL,
    STOP_INVARIANT, STOP_PROCESS
} StopReason;
