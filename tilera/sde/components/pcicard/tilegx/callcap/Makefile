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

.PHONY: all clean
all: callcap

clean:
	rm -f *.o callcap

callcap: main.o ctrl.o net.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

ifdef TILERA_ROOT
TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# NOTE: Seems to work with 6 workers.
COMMON_ARGS = \
  --mkdir /root/bin --cd /root/bin \
  --upload callcap callcap -- callcap -w 8 -n 1000000000 --link gbe1

DEV_ARGS = --hvx TLR_NETWORK=gbe1,gbe2  --hvd POST=quick \
           --hvx dataplane=1-255 --tile dataplane $(COMMON_ARGS)

.PHONY: run_dev run_sim

run_dev: callcap
	$(TILE_MONITOR) $(DEV_ARGS)

# The "input.pcap" file contains 1000 packets, and we repeat it 100 times.
ESA := mpipe0:mac=0:repeat=100:input=input.pcap:output=output.pcap:bps=40G
ESA := $(ESA):panic-on-drop
##--ESA := $(ESA):trace_mask=0xffff
ESA00 := mpipe0:mac=0:repeat=100:input=input.pcap:bps=40G:panic-on-drop
ESA10 := mpipe1:mac=0:output=output.pcap:bps=40G:panic-on-drop

SIM_ARGS := --image gx8036-dataplane $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

SIM_ARGS_GX72 := --image gx8072-dataplane $(if $(ACCURATE),,	\
	--functional) --sim-args -+- --extra-shim-args 	\
	"$(ESA00)" --extra-shim-args "$(ESA10)" -+- $(COMMON_ARGS_GX72)

SIM_DEPS := input.pcap

run_sim: callcap $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

endif
