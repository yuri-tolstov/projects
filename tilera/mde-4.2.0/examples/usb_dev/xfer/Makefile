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


OPT = -O2
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)
LIBS = -lgxio -ltmc

ud.o: ud.c ud.h
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

ud_proc.o: ud_proc.c ud.h 
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

ud_util.o: util.c util.h 
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

ud: ud.o ud_proc.o ud_util.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)


HOST_CC = gcc
HOST_OPT = -O2
HOST_CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)

ux.o: ux.c
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_XFLAGS) -c -o $@ $<

ux_util.o: util.c util.h 
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_XFLAGS) -c -o $@ $<

ux: ux.o ux_util.o
	$(HOST_CC) $(HOST_LDFLAGS) $(HOST_XFLAGS) -o $@ $^ $(HOST_LIBS)


all: ud ux

clean:
	rm -f *.o ud ux

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --hvd USB_DEV \
  --upload-tile-libs gxio \
  --mkdir /opt/test --cd /opt/test \
  --upload ud ud --quit

DEV_ARGS = $(COMMON_ARGS)

boot: ud
	$(TILE_MONITOR) $(DEV_ARGS)

run_dev:
	@echo "WARNING: This example cannot be run automatically.  Use 'make boot'"
	@echo "         to boot the target and upload the tile-side program, then"
	@echo "         consult the README for further instructions."

test_dev: run_dev

run_sim: all
	@echo "WARNING: This example cannot be run on the simulator."

test_sim: run_sim

.PHONY: run_dev test_dev run_sim test_sim

else

run:
	@echo "WARNING: This example cannot be run automatically.  Consult the README"
	@echo "         for further instructions."

test: run

.PHONY: run test

endif
