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


OPT =
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS =

EXE = hello_world

hello_world.o: hello_world.c 
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

hello_world: hello_world.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $<

all: $(EXE)

clean:
	rm -f *.o output.run $(EXE)

.PHONY: all clean


FILTER = sort

ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test \
  --cd /opt/test \
  --upload $(EXE) $(EXE) \
  --tile 2x2 -- $(EXE)

DEV_ARGS = $(COMMON_ARGS)

# For timing-accurate execution, remove the "--functional" option.
SIM_ARGS = --functional --image 2x2 $(COMMON_ARGS)

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  | $(FILTER) > output.run; \
	cat output.txt | $(FILTER) | diff output.run -

run_sim: $(EXE)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXE)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  | $(FILTER) > output.run; \
	cat output.txt | $(FILTER) | diff output.run -

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXE)
	./$(EXE)

test: $(EXE)
	set -e -o pipefail; \
	./$(EXE) \
	  | $(FILTER) > output.run; \
	cat output.txt | $(FILTER) | diff output.run -

endif
