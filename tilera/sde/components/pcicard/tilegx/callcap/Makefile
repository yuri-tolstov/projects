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
LDFLAGS = -g $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxio -ltmc
LIBMAP = -Wl,-Map=callcap.map

.PHONY: all clean
all: callcap

clean:
	rm -f *.o callcap

callcap: main.o ctrl.o net.o
	$(CC) $(LIBMAP) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

ifdef TILERA_ROOT
TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# NOTE: Seems to work with 6 workers.
COMMON_ARGS = \
  --mkdir /root/bin --cd /root/bin \
  --upload callcap callcap -- callcap

DEV_ARGS = --hvx TLR_NETWORK=gbe1,gbe2,gbe3,gbe4  --hvd POST=quick \
           --hvx dataplane=1-255 --tile dataplane $(COMMON_ARGS)

.PHONY: run_dev run_sim

run_dev: callcap
	$(TILE_MONITOR) $(DEV_ARGS)

#SIM_ARGS := --image gx8036-dataplane $(if $(ACCURATE),,--functional) \
#  --sim-args -+- --extra-shim-args "$(ESA)" -+- $(COMMON_ARGS)

SIM_ARGS := --image 4x4 --functional --hvd POST=quick $(COMMON_ARGS)

run_sim: callcap $(SIM_DEPS)
	$(TILE_MONITOR) $(SIM_ARGS)

endif
