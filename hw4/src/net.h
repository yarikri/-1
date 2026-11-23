// net.h - TCP и роли клиентов
#pragma once
#include "common.h"
enum { SHORE_LEFT, SHORE_RIGHT, OBSERVER };
int tcp_open(const char *ip, const char *port, int server);
int net_listen(const char *ip, const char *port);
void net_poll(int ms);
int net_connected(int role);
int net_take(int bank);
void net_request(int bank);
void net_event(World *w, int role, const char *line);
void net_close(void);
