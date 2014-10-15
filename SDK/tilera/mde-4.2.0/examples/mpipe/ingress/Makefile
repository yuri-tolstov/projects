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


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test \
  --upload app app -- app

DEV_ARGS = $(COMMON_ARGS) $(if $(LINK),--link $(LINK))

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev:
	echo "WARNING: This example cannot be easily tested on hardware."

ESA := mpipe0:mac=0:input=input.pcap:output=output.pcap:bps=40G:stall
##--ESA := $(ESA):trace_mask=0xffff

SIM_ARGS := --image 4x4 $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

SIM_DEPS := input.pcap

run_sim: app $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: app $(SIM_DEPS)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > sim.out; \
	diff expect.out sim.out

.PHONY: run_dev test_dev run_sim test_sim

endif
