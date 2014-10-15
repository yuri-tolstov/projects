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
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
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
PARALLELIZE = $(TILERA_ROOT)/bin/parallelize

test_dev:
	echo "WARNING: This example cannot be tested on hardware."

.PHONY: test_dev


# Run the example applications on a pair of peered simulators,
# connected via a StreamIO link between MAC 0 of the first simulator
# and MAC 1 of the second simulator.

# Use "ACCURATE=1" to avoid "--functional".
ACCURATE ?=

SIM_ARGS := --image gx8036 $(if $(ACCURATE),,--functional) --console

run_sim: app
	socket_key="pid$$$$"; \
	$(PARALLELIZE) --timeout 1800 \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-listen "@$$socket_key" \
	    $(SIM_ARGS) --huge-pages 4 \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=0:peer=trio0/1" -+- \
	    --upload $< /app \
	    --peer-barrier \
	    --run -+- /app 0 -+- \
	    --quit \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-connect "@$$socket_key" \
	    $(SIM_ARGS) --huge-pages 4 \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=1:peer=trio0/0" -+- \
	    --upload $< /app \
	    --peer-barrier \
	    --run -+- /app 1 -+- \
	    --quit

test_sim: run_sim
	echo "Successful test."

.PHONY: run_sim test_sim

endif
