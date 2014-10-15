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
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxpci -lgxio -ltmc

mmi.o: mmi.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

mmi: mmi.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: mmi

clean:
	rm -f *.o mmi

.PHONY: all clean


ifdef TILERA_ROOT

PARALLELIZE = $(TILERA_ROOT)/bin/parallelize
TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

test_dev:
	echo "WARNING: This example cannot be tested on hardware."

.PHONY: test_dev


run_sim: mmi
	socket_key="pid$$$$"; \
	$(PARALLELIZE) --timeout 1800 \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-listen "@$$socket_key" \
	    --image gx8036 --console \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=0:peer=trio0/1" -+- \
	    --upload-tile-libs gxpci \
	    --upload $< /mmi \
	    --peer-barrier \
	    --run -+- /mmi 0 -+- \
	    --peer-barrier \
	    --peer-quit \
	    --quit \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-connect "@$$socket_key" \
	    --image gx8036 --console \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=1:peer=trio0/0" -+- \
	    --upload-tile-libs gxpci \
	    --upload $< /mmi \
	    --peer-barrier \
	    --run -+- /mmi 1 -+- \
	    --peer-barrier \
	    --peer-quit \
	    --quit

test_sim: run_sim
	echo "Successful test."

.PHONY: run_sim test_sim

endif
