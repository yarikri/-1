// log.c - вывод в консоль и в лог-файл через dprintf
#include "world.h"
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

const char *const BANK[2] = { "левый", "правый" };
const char *const REFUSE_TXT[] = { "перегруз по массе", "нет места", "слишком много машин" };
const char *const DEP_TXT[] = { "паром заполнен", "вышло время ожидания",
    "следующая машина не влезает", "очередь пуста", "пустой рейс" };

void log_open(World *w) {
    w->log_fd = -1;
    if (w->cfg.log_path[0])
        w->log_fd = open(w->cfg.log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

void log_close(World *w) {
    if (w->log_fd >= 0) close(w->log_fd);
}

// печатаем и на экран, и в файл
static void vlog(World *w, int stamp, const char *fmt, va_list ap) {
    char msg[512], pre[24] = "";
    vsnprintf(msg, sizeof msg, fmt, ap);
    if (stamp) snprintf(pre, sizeof pre, "[t=%04d] ", w->now);
    dprintf(STDOUT_FILENO, "%s%s\n", pre, msg);
    if (w->log_fd >= 0) dprintf(w->log_fd, "%s%s\n", pre, msg);
    if (stamp) w->events++;
}

void logw(World *w, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vlog(w, 1, fmt, ap); va_end(ap);
}

void logr(World *w, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vlog(w, 0, fmt, ap); va_end(ap);
}
