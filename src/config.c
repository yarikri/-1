// config.c - параметры запуска: значения по умолчанию, файл, проверка
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// имя параметра -> поле в Config (везде int)
#define F(name) { #name, offsetof(Config, name) }
static const struct { const char *key; size_t off; } fields[] = {
    F(capacity_kg), F(slots), F(max_cars), F(mass_min), F(mass_max), F(size_max),
    F(arr_min), F(arr_max), F(load_time), F(unload_time), F(cross_time), F(ramp_time),
    F(max_wait), F(window), F(skip_limit), F(emergency_pct), F(priority), F(aging_limit),
    F(max_trips), F(max_time), F(delay_ms), F(seed),
    { "cars_left", offsetof(Config, cars[LEFT]) },
    { "cars_right", offsetof(Config, cars[RIGHT]) },
    { "initial_left", offsetof(Config, initial[LEFT]) },
    { "initial_right", offsetof(Config, initial[RIGHT]) },
};

void config_defaults(Config *c) {
    *c = (Config){
        .capacity_kg = 12000, .slots = 8, .max_cars = 6,
        .cars = { 12, 12 }, .initial = { 3, 3 },
        .mass_min = 700, .mass_max = 1600, .size_max = 4,
        .arr_min = 2, .arr_max = 8,
        .load_time = 2, .unload_time = 1, .cross_time = 10, .ramp_time = 1, .max_wait = 20,
        .window = 3, .skip_limit = 2,
        .emergency_pct = 10, .priority = 1, .aging_limit = 30,
        .delay_ms = 80,
    };
    snprintf(c->log_path, sizeof c->log_path, "ferry.log");
}

int config_set(Config *c, const char *key, const char *val) {
    if (!strcmp(key, "log_path")) {
        snprintf(c->log_path, sizeof c->log_path, "%s", val);
        return 0;
    }
    if (!strcmp(key, "strategy")) {
        c->strategy = !strcmp(val, "lookahead") ? 1 : atoi(val);
        return 0;
    }
    char *end;
    long v = strtol(val, &end, 10);
    if (end == val || *end) {
        dprintf(STDERR_FILENO, "Ошибка: '%s' - не число (параметр %s)\n", val, key);
        return -1;
    }
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++)
        if (!strcmp(key, fields[i].key)) {
            *(int *)((char *)c + fields[i].off) = (int)v;
            return 0;
        }
    dprintf(STDERR_FILENO, "Ошибка: неизвестный параметр '%s'\n", key);
    return -1;
}

// файл со строками вида ключ = значение, # - комментарий
int config_load_file(Config *c, const char *path) {
    char buf[8192], key[64], val[256];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(STDERR_FILENO, "Ошибка: не открывается файл '%s'\n", path);
        return -1;
    }
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    buf[n > 0 ? n : 0] = 0;
    int rc = 0;
    for (char *save, *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
        if (sscanf(line, " %63[^ =#] = %255s", key, val) == 2 && config_set(c, key, val))
            rc = -1;
    return rc;
}

// возвращает 1, если нашли ошибку
int config_validate(const Config *c) {
    const char *err = NULL;
    if (c->capacity_kg <= 0 || c->slots <= 0 || c->max_cars <= 0 || c->load_time < 1 ||
        c->unload_time < 1 || c->cross_time < 1 || c->ramp_time < 1 || c->max_wait < 1)
        err = "параметры парома и все времена должны быть > 0";
    else if (c->arr_min < 1 || c->arr_max < c->arr_min || c->mass_min < 1 || c->mass_max < c->mass_min)
        err = "неверные интервалы arr_* или mass_*";
    else if (c->size_max < 1 || c->size_max > c->slots || (long)c->size_max * c->mass_max > c->capacity_kg)
        err = "самая большая машина не поместится на паром (size_max, mass_max)";
    for (int b = 0; b < 2 && !err; b++)
        if (c->cars[b] < UNLIMITED || c->initial[b] < 0 ||
            (c->cars[b] != UNLIMITED && c->initial[b] > c->cars[b]))
            err = "неверное число машин (cars_*, initial_*)";
    if (err) dprintf(STDERR_FILENO, "Ошибка параметров: %s\n", err);
    return err != NULL;
}
