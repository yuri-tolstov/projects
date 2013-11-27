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

EXE = memory

$(EXE).o: $(EXE).c
	$(CC) $(CFLAGS) $(XFLAGS) -pthread -c -o $@ $<

$(EXE): $(EXE).o
	$(CC) $(LDFLAGS) $(XFLAGS) -pthread -o $@ $< -ltmc

all: $(EXE)

clean:
	rm -f *.o $(EXE) test_dev.out

.PHONY: all clean


FILTER = sed -e 's/= [0-9]*/= NUMBER/g'

ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --hvd STRIPE_MEMORY=never \
  --mkdir /opt/test --cd /opt/test \
  --upload $(EXE) $(EXE) -- $(EXE)

DEV_ARGS = $(COMMON_ARGS)

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  | $(FILTER) > $@.out; \
	cat output.txt | $(FILTER) | diff - $@.out

test_sim: $(EXE)
	echo "WARNING: Simulator testing not implemented yet."

.PHONY: run_dev test_dev test_sim

else

# ISSUE: Assumes booted with "STRIPE_MEMORY=never".

run: $(EXE)
	./$(EXE)

test: $(EXE)
	set -e -o pipefail; \
	./$(EXE) \
	  | $(FILTER) > $@.out; \
	cat output.txt | $(FILTER) | diff - $@.out

.PHONY: run test

endif
