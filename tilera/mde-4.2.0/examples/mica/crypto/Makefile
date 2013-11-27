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

app.o: app.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: app

clean:
	rm -f *.o app

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test \
  --upload-tile-libs gxcr --upload app app -- app

DEV_ARGS = $(COMMON_ARGS)

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: app
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  > $@.out; \
	diff expect.out $@.out

SIM_ARGS := --image gx8036 --functional $(COMMON_ARGS)

run_sim: app
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: app
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > $@.out; \
	diff expect.out $@.out

.PHONY: run_dev test_dev run_sim test_sim

else

run: app
	./$<

test: app
	set -e -o pipefail; \
	./$< \
	  > $@.out; \
	diff expect.out $@.out

.PHONY: run test

endif
