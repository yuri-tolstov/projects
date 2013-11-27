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
CFLAGS = -Wl,--fatal-warnings -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)
LIBS = -lgxpci -lgxio -ltmc

c2c_sender.o: c2c_sender.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

c2c_sender: c2c_sender.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

c2c_receiver.o: c2c_receiver.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

c2c_receiver: c2c_receiver.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

c2c_send.o: c2c_send.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

c2c_send: c2c_send.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: c2c_send c2c_sender c2c_receiver

clean:
	rm -f *.o c2c_send c2c_sender c2c_receiver

.PHONY: all clean


ifdef TILERA_ROOT

test_dev:
	echo "WARNING: This example cannot be tested on hardware."

.PHONY: test_dev


TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor
PARALLELIZE = $(TILERA_ROOT)/bin/parallelize

# Run the example c2c_sendlications on a pair of peered simulators,
# connected via a StreamIO link between MAC 0 of the first simulator
# and MAC 1 of the second simulator.

# Use "ACCURATE=1" to avoid "--functional".
ACCURATE ?=

COMMON := --image gx8036 $(if $(ACCURATE),,--functional) --console

run_sim: c2c_send
	socket_key="pid$$$$"; \
	$(PARALLELIZE) --timeout 1800 \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-listen "@$$socket_key" \
	    $(COMMON) --huge-pages 4 \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=0:peer=trio0/1" -+- \
	    --upload-tile-libs gxpci \
	    --upload $< /c2c_send \
	    --peer-barrier \
	    --run -+- /c2c_send 0 -+- \
	    --quit \
	  -++- \
	  $(TILE_MONITOR) \
	    --peer-connect "@$$socket_key" \
	    $(COMMON) --huge-pages 4 \
	    --sim-args -+- --extra-shim-args \
	      "trio0:mac=1:peer=trio0/0" -+- \
	    --upload-tile-libs gxpci \
	    --upload $< /c2c_send \
	    --peer-barrier \
	    --run -+- /c2c_send 1 -+- \
	    --quit

test_sim: run_sim
	echo "Successful test."

.PHONY: run_sim test_sim

endif
