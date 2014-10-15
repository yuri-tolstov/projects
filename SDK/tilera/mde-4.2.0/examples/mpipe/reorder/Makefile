# Default target.
all:

ifdef TILERA_ROOT

CC = $(TILERA_ROOT)/bin/tile-gcc
MPIPE_CC = $(TILERA_ROOT)/bin/tile-mpipe-cc
OBJDUMP = $(TILERA_ROOT)/bin/tile-objdump

else

ifeq ($(filter tile%,$(shell uname -m)),)
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = gcc
MPIPE_CC = tile-mpipe-cc

endif


OPT = -O3
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxio -ltmc


app.o: app.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: app

app.objdump: app
	$(OBJDUMP) -d app > $@

clean:
	rm -f *.o app app.objdump

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# The "input.pcap" file contains 1000 packets, and we repeat it 100 times.

WORKERS ?= 8
NUM ?= 100000
LINK ?= gbe0

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test --upload app app \
  --tile dataplane -- app -w $(WORKERS) -n $(NUM) --link $(LINK)

DEV_ARGS = --hvx dataplane=1-255 $(COMMON_ARGS)

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: app
	echo "WARNING: This example cannot be tested on hardware."

ESA := mpipe0:mac=0:repeat=100:input=input.pcap:output=output.pcap:bps=40G
ESA := $(ESA):panic-on-drop
##--ESA := $(ESA):trace_mask=0xffff

SIM_ARGS := --image gx8036-dataplane $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

SIM_DEPS := input.pcap

run_sim: app $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: app $(SIM_DEPS)
	set -e -o pipefail; \
	/usr/sbin/tshark -r input.pcap \
	| sed -e 's:^ *[0-9]* *[0-9.]* *::' > input.brief1; \
	rm -f input.brief; \
	for x in `seq 1 100`; do cat input.brief1 >> input.brief; done; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > sim.out; \
	/usr/sbin/tshark -r output.pcap \
	| sed -e 's:^ *[0-9]* *[0-9.]* *::' > output.brief; \
	diff input.brief output.brief; \
	diff /dev/null sim.out


.PHONY: run_dev test_dev run_sim test_sim

endif
