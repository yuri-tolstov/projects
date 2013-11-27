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

OPT = -O3
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxpci -lgxio -ltmc

tile_tap: tile_tap.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

EXES = tile_tap

all: $(EXES)

clean:
	rm -f *.o $(EXES) $(HOST_EXES)

.PHONY: all clean


ifdef TILERA_ROOT

HOST_CC = cc
HOST_INC = $(TILERA_ROOT)/include
HOST_OPT = -O2
HOST_CFLAGS = -std=gnu99 -Wall -Werror -g $(HOST_OPT) -I$(HOST_INC) \
  -DTILEPCI_HOST -pthread
HOST_CARD = \
  $(strip $(subst /gxpci, , $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)))

host_tap: host_tap.c
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $<

HOST_EXES = host_tap

all: $(HOST_EXES)

run_dev test_dev: $(EXES)
	echo "WARNING: This example cannot be executed directly."

.PHONY: run_dev test_dev

run_sim test_sim: $(EXES)
	echo "WARNING: This example cannot be run on the simulator."

.PHONY: run_sim test_sim

endif
