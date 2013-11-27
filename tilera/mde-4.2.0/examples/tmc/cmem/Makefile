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

EXE = simple

OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)

$(EXE).o: $(EXE).c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE): $(EXE).o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

all: $(EXE)

clean:
	rm -f *.o $(EXE)

.PHONY: all clean


FILTER = sed -e 's/0x[0-9a-f][0-9a-f]*/HEX/g' -e 's/[0-9][0-9]*/NUM/g'

ifdef TILERA_ROOT

SIZE = 2x2

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --huge-pages 4 \
  --mkdir /opt/test --cd /opt/test \
  --upload $(EXE) $(EXE) -- $(EXE)

DEV_ARGS = --tile $(SIZE) $(COMMON_ARGS)

SIM_ARGS = --image $(SIZE) --functional $(COMMON_ARGS)

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  | $(FILTER) > $@.out; \
	cat $(EXE).out | $(FILTER) | diff $@.out -

run_sim: $(EXE)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > $@.out; \
	$(FILTER) < $@.out > $@.filter-out; \
	$(FILTER) < $(EXE).out > $(EXE).filter-out; \
	diff $(EXE).filter-out $@.filter-out

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXE)
	./$(EXE)

test: $(EXE)
	set -e -o pipefail; \
	./$(EXE) \
	  | $(FILTER) > $@.out; \
	cat $(EXE).out | $(FILTER) | diff $@.out -

.PHONY: run test

endif
