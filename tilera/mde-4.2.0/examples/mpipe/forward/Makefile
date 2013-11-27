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

app_gx8072.o: app_gx8072.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $< -DGXIO_MULTI_MPIPES

app_gx8072: app_gx8072.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: app app_gx8072

clean:
	rm -f *.o app app_gx8072

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# NOTE: Seems to work with 6 workers.
WORKERS := 8

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test --upload app app \
  --tile dataplane -- app -w $(WORKERS) -n 100000

COMMON_ARGS_GX72 = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test --upload app_gx8072 app \
  --tile dataplane -- app -w $(WORKERS) -n 100000

DEV_ARGS = --hvx dataplane=1-255 $(COMMON_ARGS) $(if $(LINK),--link $(LINK))

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev:
	echo "WARNING: This example cannot be easily tested on hardware."

gx8072_image:
	$(TILE_MONITOR) --simulator --config gx8072 --functional --console \
	--hvx dataplane=1-71 --sim-args - --backtrace-limit 1000 - \
	--create-image $(TILERA_ROOT)/lib/boot/gx8072-dataplane.imag

$(TILERA_ROOT)/lib/boot/gx8072-dataplane.image:
	$(TILE_MONITOR) --simulator --config gx8072 --functional --console \
	--hvx dataplane=1-71 --sim-args - --backtrace-limit 1000 - \
	--create-image $(TILERA_ROOT)/lib/boot/gx8072-dataplane.image

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

run_sim: app $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

run_sim_gx8072: app_gx8072 $(SIM_DEPS) \
			 $(TILERA_ROOT)/lib/boot/gx8072-dataplane.image
	$(TILE_MONITOR) $(SIM_ARGS_GX72)

# FIXME: Verify "output.pcap".
test_sim: app $(SIM_DEPS)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > sim.out; \
	diff expect.out sim.out


.PHONY: run_dev test_dev run_sim run_sim_gx8072 test_sim gx8072_image

endif
