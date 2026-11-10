CC     = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -O2 -pthread
LDLIBS = -pthread -lrt
CORE   = src/car.c src/config.c src/dispatcher.c src/ferry.c src/log.c src/stats.c src/sync.c
HEADERS = $(wildcard src/*.h)
PROGRAMS = ferry_unnamed ferry_named ferry_independent ferry_process dispatcher_process cars_process

.PHONY: all test clean
all: $(PROGRAMS)

ferry_unnamed: src/main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DBUILD_KIND=0 -o $@ src/main.c $(CORE) $(LDLIBS)
ferry_named: src/main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DBUILD_KIND=1 -o $@ src/main.c $(CORE) $(LDLIBS)
ferry_independent: src/main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DBUILD_KIND=2 -o $@ src/main.c $(CORE) $(LDLIBS)
ferry_process: src/actor_main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DACTOR_ROLE=ROLE_FERRY -o $@ src/actor_main.c $(CORE) $(LDLIBS)
dispatcher_process: src/actor_main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DACTOR_ROLE=ROLE_DISPATCHER -o $@ src/actor_main.c $(CORE) $(LDLIBS)
cars_process: src/actor_main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DACTOR_ROLE=ROLE_CARS -o $@ src/actor_main.c $(CORE) $(LDLIBS)

test: all
	bash run_tests.sh
clean:
	rm -f $(PROGRAMS) logs/*.log logs/*.out
