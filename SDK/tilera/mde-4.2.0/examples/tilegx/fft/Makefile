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


HEADERS = fft_1024.h fft_1024_input_rev.h fft_1024_twiddles.h
EXE = fft_1024

OPT = -O3 -funroll-all-loops -funsafe-loop-optimizations
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)

$(EXE).o: $(EXE).c $(HEADERS)
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE)_main.o: $(EXE)_main.c $(HEADERS)
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE): $(EXE).o $(EXE)_main.o
	$(CC) $(LDFLAGS) $(XFLAGS) $^ -o $@

all: $(EXE)

clean:
	rm -f *.o *.filter-out test_sim.out $(EXE)

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

SIZE = 2x2

FILTER = sed 's/[0-9]* cycles/N cycles/g'

COMMON_ARGS = \
  --try-console-out console.out \
  --env LD_CACHE_HASH=none --mkdir /opt/test --cd /opt/test \
  --upload $(EXE) $(EXE) -- $(EXE)

DEV_ARGS := $(COMMON_ARGS)

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  >& $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	$(FILTER) < $(EXE).out > $(EXE).filter-out; \
	diff $(EXE).filter-out $@.filter-out

SIM_ARGS := --image $(SIZE) $(if $(ACCURATE),,--functional) $(COMMON_ARGS)

run_sim: $(EXE)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  >& $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	$(FILTER) < $(EXE).out > $(EXE).filter-out; \
	diff $(EXE).filter-out $@.filter-out

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXE)
	./$<

test: $(EXE)
	set -e -o pipefail; \
	./$< \
	  >& $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	$(FILTER) < $(EXE).out > $(EXE).filter-out; \
	diff $(EXE).filter-out $@.filter-out

.PHONY: run test

endif
