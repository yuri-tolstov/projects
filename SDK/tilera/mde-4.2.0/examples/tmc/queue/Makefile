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

EXE = bench_one_to_one

OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = -pthread

# Choose USE_BASIC_QUEUE or USE_CACHELINE_QUEUE.
EXTRA_FLAGS = -DUSE_CACHELINE_QUEUE

TEST_NUM = 1000
SIM_TEST_NUM = 10

$(EXE).o: $(EXE).c
	$(CC) $(CFLAGS) $(XFLAGS) $(EXTRA_FLAGS) -c -o $@ $<

$(EXE): $(EXE).o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

all: $(EXE)

clean:
	rm -f *.o $(EXE)

.PHONY: all clean


FILTER = sed -e 's/0x[0-9a-f][0-9a-f]*/HEX/g' -e 's/[0-9][0-9]*/NUM/g'

expect.filter-out: expect.out
	$(FILTER) < $< > $@

ifdef TILERA_ROOT

SIZE = 2x2

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --hvx TLR_NETWORK=none \
  --mkdir /opt/test --cd /opt/test \
  --upload $(EXE) $(EXE) -- $(EXE)

DEV_ARGS = --tile $(SIZE) $(COMMON_ARGS) --test_num=$(TEST_NUM)

# For faster execution, add "--functional".
SIM_ARGS = --image $(SIZE) $(COMMON_ARGS) --test_num=$(SIM_TEST_NUM)

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE) expect.filter-out
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  > $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	diff expect.filter-out $@.filter-out

run_sim: $(EXE)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXE) expect.filter-out
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	diff expect.filter-out $@.filter-out

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXE)
	./$(EXE) --test_num=$(TEST_NUM)

test: $(EXE) expect.filter-out
	set -e -o pipefail; \
	./$(EXE) --test_num=$(TEST_NUM) \
	  > $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	diff expect.filter-out $@.filter-out

.PHONY: run test

endif
