// config.h - параметры запуска
#pragma once
typedef struct {
    int capacity_kg, slots, max_cars;       // ограничения парома
    int cars[2], initial[2];                // машин всего / уже стоят в начале
    int mass_min, mass_max, size_max;       // масса на одно место, макс. размер
    int arr_min, arr_max;                   // интервал приезда машин
    int load_time, unload_time, cross_time, ramp_time, max_wait;
    int strategy, window, skip_limit;       // 0 - fifo, 1 - lookahead
    int emergency_pct, priority, aging_limit;
    int max_trips, max_time;                // 0 - без ограничения
    int delay_ms, seed;
    char log_path[256];
} Config;

void config_defaults(Config *c);
int config_load_file(Config *c, const char *path);
int config_set(Config *c, const char *key, const char *val);
int config_validate(const Config *c);

