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

simple.tile.o: simple.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

simple.tile: simple.tile.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: simple.tile

clean:
	rm -f *.o simple.tile simple.host

.PHONY: all clean


ifdef TILERA_ROOT

HOST_CC = gcc
HOST_INC = 
HOST_OPT = -O2
HOST_CFLAGS = -std=gnu99 -Wall -Werror -g $(HOST_OPT) \
  -I$(TILERA_ROOT)/include -DTILEPCI_HOST
HOST_CARD = \
  $(strip $(subst /gxpci, , $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)))

simple.host: simple.c
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $<

all: simple.host


EXES = simple.tile simple.host

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

TEST_ARGS = --queue=0 --size=4096

COMMON_ARGS = \
  --try-console-out console.out \
  --upload-tile-libs gxpci \
  --upload simple.tile simple.tile \
  -- simple.tile $(TEST_ARGS)

DEV_ARGS = $(COMMON_ARGS)

ifneq ($(filter /gxpci%, $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)),)

run_dev test_dev: $(EXES)
	set -e; \
	$(TILE_MONITOR) --try-console-out console.out --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 30 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	  -++- ./simple.host $(TEST_ARGS) --card=$(HOST_CARD)

else

run_dev test_dev: $(DEPS)
	echo "WARNING: This example requires pci."

endif

run_sim test_sim: $(EXES)
	echo "WARNING: This example cannot be run on the simulator."

.PHONY: run_dev test_dev run_sim test_sim

endif
