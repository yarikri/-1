// log.h - вывод в консоль и в лог-файл
#pragma once
#include "common.h"

extern const char *const BANK[2];
extern const char *const REFUSE_TXT[];
extern const char *const DEP_TXT[];

#include <stdbool.h>
int log_open(World *w, bool create);
void log_close(void);
void logw(World *w, const char *fmt, ...);   // со временем
void logr(World *w, const char *fmt, ...);   // без времени

