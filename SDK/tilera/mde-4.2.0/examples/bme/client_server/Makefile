# Default target.
all:

ifndef TILERA_ROOT
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = $(TILERA_ROOT)/bin/tile-gcc

# Use -Wno-format to tell gcc not to worry about "%#'" format prefixes.
OPT = -O2
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -Wno-format

# ISSUE: Support "XFLAGS"?

bme_server: bme_server.c msg.h
	$(CC) -mbme $(CFLAGS) -o $@ bme_server.c

linux_client: linux_client.c msg.h
	$(CC) $(CFLAGS) -o $@ linux_client.c -ltmc

# We generate an example input file from one of our source files; the
# processing done by the example reverses what we're doing here, so the
# result should be identical to the original source.
#
example.in: linux_client.c
	tr <$? >$@ '!-~' 'P-~!-O'

all: bme_server linux_client example.in

clean:
	rm -f bme_server linux_client example.in example.out

.PHONY: all clean


TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --console \
  --bme bme_server \
  --upload linux_client /linux_client \
  --upload example.in /example.in \
  --run - /linux_client -i /example.in -o /example.out 0,0 - \
  --download /example.out example.out \
  --quit

DEV_ARGS = --hvc pci.hvc $(COMMON_ARGS)

SIM_ARGS = --simulator --config 4x4 \
  --shim "0:1:rw:memory:1" --shim "3:1:e:memory:2" \
  --hvc sim.hvc \
  --functional --standard-symbols $(COMMON_ARGS)

DEPS = example.in linux_client bme_server


run_dev: $(DEPS) pci.hvc
	set -e; \
	rm -f example.out; \
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(DEPS) pci.hvc
	set -e; \
	rm -f example.out; \
	$(TILE_MONITOR) $(DEV_ARGS); \
	diff example.out linux_client.c

run_sim: $(DEPS) sim.hvc
	set -e; \
	rm -f example.out; \
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(DEPS) sim.hvc
	set -e; \
	rm -f example.out; \
	$(TILE_MONITOR) $(SIM_ARGS); \
	diff example.out linux_client.c

.PHONY: run_dev run_sim test_dev test_sim
