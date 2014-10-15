# Default target.
all:

ifdef TILERA_ROOT

CC = $(TILERA_ROOT)/bin/tile-gcc
MPIPE_CC = $(TILERA_ROOT)/bin/tile-mpipe-cc

else

ifeq ($(filter tile%,$(shell uname -m)),)
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = gcc
MPIPE_CC = tile-mpipe-cc

endif


OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxio -ltmc

app.o: app.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: app

clean:
	rm -f *.o app

.PHONY: all clean


RUN_ARGS = --tile dataplane -- app -w 2 -n 10000

ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test \
  --upload app app $(RUN_ARGS)

DEV_ARGS = --hvx dataplane=1-255 $(COMMON_ARGS) $(if $(LINK),--link $(LINK))

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: run_dev

# Use the "loop0" link (channel 16).
ESA := mpipe0:mac=16:bps=10G:panic-on-drop

SIM_ARGS := --image gx8036-dataplane $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

run_sim: app
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: run_sim

.PHONY: run_dev test_dev run_sim test_sim

else

run: app
	shepherd $(RUN_ARGS)

test: run

.PHONY: run test

endif
