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
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = -pthread

EXE = spinner

$(EXE).o: $(EXE).c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE): $(EXE).o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

all: $(EXE)

clean:
	rm -f *.o $(EXE) $(IMAGE_FILE) test_dev.out test_sim.out

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

# The --hvx switch allows us to add to the linux boot parameters.  In
# this case, we change the boot configuration to have tiles 1-55 run
# in 'dataplane' mode.  (In the current release, tile 0 has some overhead
# that makes it a poor choice for dataplane.)
BOOT_ARGS = --hvx "dataplane=1-55"

CONFIG = 8x8

IMAGE_FILE = $(CONFIG)-dataplane.image

$(IMAGE_FILE):
	@echo Building dataplane image file: this will take about ten minutes.
	$(TILE_MONITOR) --simulator --config $(CONFIG) --functional --console \
	  --create-image $@.tmp $(BOOT_ARGS)
	mv $@.tmp $@

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test --upload $(EXE) $(EXE)

DEV_ARGS = $(BOOT_ARGS) $(COMMON_ARGS) -- $(EXE) 10

SIM_ARGS = --image-file $(IMAGE_FILE) --functional $(COMMON_ARGS) -- $(EXE) 1

run_dev: $(EXE)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXE)
	set -e; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  > $@.out; \
	diff output.txt $@.out

run_sim: $(EXE) $(IMAGE_FILE)
	@echo warning: this test takes many hours to run.
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim:
	echo "WARNING: This example is too slow for the simulator."
	echo "For a multi-hour simulator test, 'make test_sim_slow'."

test_sim_slow: $(EXE) $(IMAGE_FILE)
	@echo warning: this test takes many hours to run.
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > test_sim.out; \
	diff output.txt test_sim.out

.PHONY: run_dev test_dev test_sim test_sim_slow

else

run: $(EXE)
	./$< 10

test: $(EXE)
	set -e; \
	./$< 10 \
	  > $@.out; \
	diff output.txt $@.out

.PHONY: run test

endif
