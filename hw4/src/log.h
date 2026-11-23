// log.h - вывод в консоль и в лог-файл
#pragma once
#include "common.h"

extern const char *const BANK[2];
extern const char *const REFUSE_TXT[];
extern const char *const DEP_TXT[];

void log_open(World *w);
void log_close(World *w);
void logw(World *w, const char *fmt, ...);   // со временем
void logr(World *w, const char *fmt, ...);   // без времени

