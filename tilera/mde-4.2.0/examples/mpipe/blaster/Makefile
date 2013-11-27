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
CFLAGS = -std=gnu99 -Wall -Werror -Wshadow -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)
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
  --mkdir /opt/test --cd /opt/test \
  --upload app app \
  --upload input.pcap input.pcap

DEV_ARGS = --hvx dataplane=1-255 $(COMMON_ARGS)

DEV_RUN_ARGS = \
  taskset -c 1 ./app -n 1000000000 --wait \
  --link $(if $(LINK),$(LINK),xgbe4) input.pcap

run_dev: app
	$(TILE_MONITOR) $(DEV_ARGS) -- $(DEV_RUN_ARGS)

# ISSUE: To actually verify results requires a "capture" app,
# but we can at least verify the "mpipe-stat" output.

# This version assumes xgbe3 and xgbe4 are directly connected to each other.
test_dev_2port: app
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  --console-out dev.console \
	  --upload-tile /usr/sbin/mpipe-link \
	  --run - mpipe-link -u xgbe3 - \
	  --run - $(DEV_RUN_ARGS) - \
	  --system 'mpipe-stat -v xgbe3 >& dev-xgbe3.out' \
	  --system 'mpipe-stat -v xgbe4 >& dev-xgbe4.out' \
	  --download-same dev-xgbe3.out \
	  --download-same dev-xgbe4.out \
	  --quit \
	  > dev.out; \
	diff /dev/null dev.out; \
	grep "Frames transmitted *1000000000" dev-xgbe4.out; \
	grep "Frames received *1000000000" dev-xgbe3.out

# This version uses PHY loopback mode so it works without a connection
# between xgbe3 and xgbe4.
test_dev_1port: app
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  --console-out dev.console \
	  --upload-tile /usr/sbin/mpipe-link \
	  --run - mpipe-link -u -p xgbe4 - \
	  --run - $(DEV_RUN_ARGS) - \
	  --system 'mpipe-stat -v xgbe4 >& dev-xgbe4.out' \
	  --download-same dev-xgbe4.out \
	  --quit \
	  > dev.out; \
	diff /dev/null dev.out; \
	grep "Frames transmitted *1000000000" dev-xgbe4.out; \
	grep "Frames received *1000000000" dev-xgbe4.out

test_dev: test_dev_1port

ESA := mpipe0:mac=0:output=output.pcap:bps=10G

SIM_ARGS := --image gx8036-dataplane $(if $(ACCURATE),,--functional) \
  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS) \
  -- taskset -c 1 ./app --wait -n 100000 input.pcap

run_sim: app
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: app
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
