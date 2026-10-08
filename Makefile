CC     = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2
SRC    = $(wildcard src/*.c)

ferry: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: ferry
	./run_tests.sh

clean:
	rm -f ferry logs/*.log
