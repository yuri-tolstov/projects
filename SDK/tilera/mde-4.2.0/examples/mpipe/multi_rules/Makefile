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


# HACK: Assume between 100 and 900 packets for each "dynamic" tile.
FILTER = sort | \
  sed -e '/Cpu [45]/s/handled  ... packets/handled  500 packets/'

ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --huge-pages 16 \
  --mkdir /opt/test --cd /opt/test \
  --upload app app -- app

DEV_ARGS = $(COMMON_ARGS)

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev:
	echo "WARNING: This example cannot be easily tested on hardware."

ESA := mpipe0:mac=1:input=input.pcap:bps=1G:stall
ESA := $(ESA):mac=2:input=input.pcap:bps=1G:stall
ESA := $(ESA):mac=3:input=input.pcap:bps=1G:stall

SIM_ARGS := --image gx8036 $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

SIM_DEPS := input.pcap

run_sim: app $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: app $(SIM_DEPS)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > sim.out; \
	cat sim.out | $(FILTER) > sim.filter-out; \
	diff expect.out sim.filter-out

.PHONY: run_dev test_dev run_sim test_sim

endif
