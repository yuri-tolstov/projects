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

EXE = single_app

SRCS = $(EXE).c parallel_utils.c
OBJS = $(SRCS:%.c=%.o)

OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS =

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ -ltmc

all: $(EXE)

clean:
	rm -f *.o $(EXE) test_dev.out test_sim.out
	@echo Done.

.PHONY: all clean


FILTER = sed -e 's/(pid=[0-9]*)/(pid=nnn)/g' | sort

ifdef TILERA_ROOT

SIZE = 4x4

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
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
	diff -u $(EXE).out $@.out

run_sim: $(EXE)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  | $(FILTER) > $@.out; \
	diff -u $(EXE).out $@.out

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXE)
	./$(EXE)

test: $(EXE)
	set -e -o pipefail; \
	./$(EXE) \
	  | $(FILTER) > $@.out; \
	 diff -u $(EXE).out $@.out

.PHONY: run test

endif
