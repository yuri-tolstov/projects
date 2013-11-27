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

OPT = -O3
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxpci -lgxio -ltmc

tile_h2t: h2t.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

tile_t2h: t2h.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

tile_dup: duplex.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

EXES = tile_t2h tile_h2t tile_dup

all: $(EXES)

clean:
	rm -f *.o $(EXES) $(HOST_EXES)

.PHONY: all clean


ifdef TILERA_ROOT

HOST_CC = cc
HOST_INC = $(TILERA_ROOT)/include
HOST_OPT = -O2
HOST_CFLAGS = -std=gnu99 -Wall -Werror -g $(HOST_OPT) -I$(HOST_INC) \
  -DTILEPCI_HOST -pthread
HOST_CARD = \
  $(strip $(subst /gxpci, , $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)))

host_t2h: host.c
	$(HOST_CC) -DTEST_T2H $(HOST_CFLAGS) -o $@ $<

host_h2t: host.c
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $<

host_loopback: host_loopback.c
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $<

HOST_EXES = host_t2h host_h2t host_loopback

all: $(HOST_EXES)

PKT_SIZE ?= 64

ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = --try-console-out console.out --upload-tile-libs gxpci
BOOT_ARGS = --hvd POST=quick --hvx TLR_NETWORK=none

DEV_ARGS = $(COMMON_ARGS)

ifneq ($(filter /gxpci%, $(shell $(TILERA_ROOT)/bin/tile-dev --quiet)),)

test_t2h: $(EXES) $(HOST_EXES)
	set -e; \
	$(TILE_MONITOR) $(BOOT_ARGS) --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 1200 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	    --upload ./tile_t2h ./t2h -- ./t2h --pkt_size=$(PKT_SIZE) \
	  -++- ./host_t2h --card=$(HOST_CARD)  --packet_size=$(PKT_SIZE)

test_h2t: $(EXES) $(HOST_EXES)
	set -e; \
	$(TILE_MONITOR) $(BOOT_ARGS) --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 1200 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	    --upload ./tile_h2t ./h2t -- ./h2t --pkt_size=$(PKT_SIZE) \
	  -++- ./host_h2t --card=$(HOST_CARD) --packet_size=$(PKT_SIZE)

test_loopback: $(EXES) $(HOST_EXES)
	set -e; \
	$(TILE_MONITOR) $(BOOT_ARGS) --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 1200 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	    --upload ./tile_dup ./dup -- ./dup --pkt_size=$(PKT_SIZE) \
	  -++- ./host_loopback --card=$(HOST_CARD) --packet_size=$(PKT_SIZE)

test_t2h_32B: $(EXES) $(HOST_EXES)
	set -e; \
	$(TILE_MONITOR) $(BOOT_ARGS) --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 1200 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	    --upload ./tile_t2h ./t2h -- ./t2h --pkt_size=32 \
	  -++- ./host_t2h --card=$(HOST_CARD) \
	    --host_buf_size=32 --packet_size=32

test_h2t_32B: $(EXES) $(HOST_EXES)
	set -e; \
	$(TILE_MONITOR) $(BOOT_ARGS) --quit; \
	$(TILERA_ROOT)/bin/parallelize --timeout 1200 \
	  -++- $(TILE_MONITOR) --resume $(DEV_ARGS) \
	    --upload ./tile_h2t ./h2t -- ./h2t --pkt_size=32 \
	  -++- ./host_h2t --card=$(HOST_CARD) \
	    --host_buf_size=32 --packet_size=32

test_dev:
	make test_t2h_32B
	make test_h2t_32B

else

test_dev test_t2h test_h2t test_loopback test_t2h_32B test_h2t_32B: $(EXES)
	echo "WARNING: This example requires pci."

endif

.PHONY: test_dev test_t2h test_h2t test_loopback test_t2h_32B test_h2t_32B

endif


run_sim test_sim: $(EXES)
	echo "WARNING: This example cannot be run on the simulator."

.PHONY: run_sim test_sim

endif
