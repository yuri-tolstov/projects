# Default target.
all:

ifdef TILERA_ROOT

CC = $(TILERA_ROOT)/bin/tile-gcc

else

ifeq ($(filter tile%,$(shell uname -m)),)
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = gcc

endif


OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxcr -lgxio -ltmc

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app_poll.o: app.c queue.h
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app_int.o: app.c queue.h
	$(CC) $(CFLAGS) $(XFLAGS) -DINTERRUPT_MODEL=1 -c -o $@ $<

app_poll: app_poll.o queue.o
	$(CC) $(LDFLAGS) $(XFLAGS) $^ -o $@ $(LIBS)

app_int: app_int.o queue.o
	$(CC) $(LDFLAGS) $(XFLAGS) $^ -o $@ $(LIBS)

all: app_poll app_int

clean:
	rm -f *.o app_poll app_int

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# ISSUE: Perhaps a single EXE with an "--int" flag would be cleaner?

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test \
  --upload-tile-libs gxcr --upload app_poll app_poll -- app_poll

DEV_ARGS = $(COMMON_ARGS)

# ISSUE: What about "app_int"?

run_dev: app_poll
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: run_dev

run_sim:
	echo "WARNING: This example cannot be run on the simulator."

test_sim: run_sim

.PHONY: run_dev test_dev run_sim test_sim

else

run: app_poll
	./$<

test: run

.PHONY: run test

endif
