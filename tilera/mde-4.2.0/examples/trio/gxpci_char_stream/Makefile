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
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)
LIBS = -lgxpci -lgxio -ltmc

bandwidth.tile.o: bandwidth.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

bandwidth.tile: bandwidth.tile.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: bandwidth.tile

EXES = bandwidth.tile bandwidth.host

clean:
	rm -f *.o $(EXES)

.PHONY: all clean


ifdef TILERA_ROOT

HOST_CC = gcc
HOST_INC = $(TILERA_ROOT)/include
HOST_OPT = -O2
HOST_CFLAGS = -std=gnu99 -Wall -Werror -g $(HOST_OPT) -I$(HOST_INC) \
  -DTILEPCI_HOST
HOST_CARD = \
  $(strip $(subst /gxpci, , $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)))

bandwidth.host: bandwidth.c
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $<

all: bandwidth.host


TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

TEST_ARGS = --chan=1 --size=4096

COMMON_ARGS = \
  --try-console-out console.out \
  --upload-tile-libs gxpci \
  --upload bandwidth.tile bandwidth.tile \
  -- bandwidth.tile $(TEST_ARGS)

DEV_ARGS = $(COMMON_ARGS)

ifneq ($(filter /gxpci%, $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)),)

run_dev test_dev: $(EXES)
	set -e; \
        $(TILE_MONITOR) --try-console-out console.out --quit; \
        $(TILERA_ROOT)/bin/parallelize --timeout 30 \
          -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
          -++- ./bandwidth.host $(TEST_ARGS) --card=$(HOST_CARD) -++-

else

run_dev test_dev: $(DEPS)
	echo "WARNING: This example requires pci."

endif

run_sim test_sim: $(EXES)
	echo "WARNING: This example cannot be run on the simulator."

.PHONY: run_dev test_dev run_sim test_sim

endif
