// dispatcher.h - диспетчер (отдельный процесс)
#pragma once
#include "common.h"
#include "car.h"

typedef enum { D_WAIT, D_LOAD, D_DEPART } DispAction;
typedef enum { DEP_FULL, DEP_TIMEOUT, DEP_BLOCKED, DEP_NO_MORE, DEP_EMPTY } DepReason;

typedef struct { DispAction act; Car *car; DepReason reason; } Decision;

Decision dispatcher_decide(World *w);   // семафор доступа должен быть занят
void dispatcher_process(World *w);
