// dispatcher.h - диспетчер
#pragma once
#include "common.h"

typedef enum { D_WAIT, D_LOAD, D_DEPART } DispAction;
typedef enum { DEP_FULL, DEP_TIMEOUT, DEP_BLOCKED, DEP_NO_MORE, DEP_EMPTY } DepReason;

typedef struct { DispAction act; int car; DepReason reason; } Decision;

Decision dispatcher_decide(World *w);

