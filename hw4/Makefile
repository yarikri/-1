CC = gcc
CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -std=c11 -Wall -Wextra -O2
MODEL = src/main.c src/car.c src/config.c src/dispatcher.c src/ferry.c src/log.c src/stats.c
HEADERS = $(wildcard src/*.h)

.PHONY: all test clean
all: server shore observer

server: $(MODEL) src/net.c src/tcp.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
shore: src/client.c src/tcp.c src/net.h src/common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
observer: src/client.c src/tcp.c src/net.h src/common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DWATCH -o $@ $(filter %.c,$^)
test: all
	bash run_tests.sh
clean:
	rm -f server shore observer
