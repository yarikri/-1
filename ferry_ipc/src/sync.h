// sync.h - общая память, семафоры и время
#pragma once
#include <semaphore.h>
#include <stdbool.h>
#include <signal.h>
#include "common.h"
enum { S_LOCK, S_DEPART, S_UNLOADED, SEM_COUNT, S_CAR = SEM_COUNT };
enum { ROLE_FERRY, ROLE_DISPATCHER, ROLE_CARS };
extern volatile sig_atomic_t interrupted;
long now_ms(void);
int now_ticks(World *w);
void sleep_ticks(World *w, int n);
bool is_quit(World *w);
bool wait_sem(World *w, int index);
void post_sem(int index);
void world_lock(World *w);
void world_unlock(World *w);
void signals_init(void);
int ipc_create(bool named, bool independent, const Config *cfg);
int ipc_attach(void);
World *ipc_world(void);
Car *ipc_car(int slot);
Car *ipc_car_create(int slot);
void ipc_close(bool owner);
void ipc_wake_all(void);
void actor_run(World *w, int role);
