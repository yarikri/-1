// log.c - вывод в консоль и в лог-файл через dprintf
// logw вызываем при занятом семафоре доступа, поэтому строки не перемешиваются
#include "world.h"
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static int log_fd = -1;     // дескриптор свой у каждого процесса

const char *const BANK[2] = { "левый", "правый" };
const char *const REFUSE_TXT[] = { "перегруз по массе", "нет места", "слишком много машин" };
const char *const DEP_TXT[] = { "паром заполнен", "вышло время ожидания",
    "следующая машина не влезает", "очередь пуста", "пустой рейс" };

int log_open(World *w, bool create) {
    log_fd = -1;
    if (w->cfg.log_path[0])
        log_fd = open(w->cfg.log_path, O_WRONLY | O_APPEND | (create ? O_CREAT | O_TRUNC : 0), 0644);
    return w->cfg.log_path[0] && log_fd < 0 ? -1 : 0;
}

void log_close(void) {
    if (log_fd >= 0) close(log_fd);
}

// печатаем и на экран, и в файл
static void vlog(World *w, int stamp, const char *fmt, va_list ap) {
    char msg[512], pre[24] = "";
    vsnprintf(msg, sizeof msg, fmt, ap);
    if (stamp) snprintf(pre, sizeof pre, "[t=%04d] ", now_ticks(w));
    dprintf(STDOUT_FILENO, "%s%s\n", pre, msg);
    if (log_fd >= 0) dprintf(log_fd, "%s%s\n", pre, msg);
}

void logw(World *w, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vlog(w, 1, fmt, ap); va_end(ap);
}

void logr(World *w, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vlog(w, 0, fmt, ap); va_end(ap);
}
