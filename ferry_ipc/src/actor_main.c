// actor_main.c - независимо запускаемый участник
#include "world.h"
#include <stdio.h>

int main(void) {
    signals_init();
    if (ipc_attach()) return 2;
    World *w = ipc_world();
    if (log_open(w, false)) { perror("log"); ipc_close(false); return 2; }
    actor_run(w, ACTOR_ROLE);
    log_close();
    ipc_close(false);
    return 0;
}
