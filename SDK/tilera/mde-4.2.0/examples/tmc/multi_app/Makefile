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

EXE = start

EXES = $(EXE) app1 app2

$(EXE).o: $(EXE).c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

$(EXE): $(EXE).o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

app.o: app.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app1: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

app2: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -ltmc

all: $(EXES)

clean:
	rm -f *.o $(EXES)

.PHONY: all clean


ifdef TILERA_ROOT

SIZE = 4x4

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test \
  $(foreach exe, $(EXES), --upload $(exe) $(exe)) \
  -- $(EXE)

DEV_ARGS = --tile $(SIZE) $(COMMON_ARGS)

SIM_ARGS = --image $(SIZE) --functional $(COMMON_ARGS)

run_dev: $(EXES)
	$(TILE_MONITOR) $(DEV_ARGS)

test_dev: $(EXES)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(DEV_ARGS) \
	  > $@.out; \
	diff $(EXE).out $@.out

run_sim: $(EXES)
	$(TILE_MONITOR) $(SIM_ARGS)

test_sim: $(EXES)
	set -e -o pipefail; \
	$(TILE_MONITOR) $(SIM_ARGS) \
	  > $@.out; \
	diff $(EXE).out $@.out

.PHONY: run_dev test_dev run_sim test_sim

else

run: $(EXES)
	./$(EXE)

test: $(EXES)
	set -e -o pipefail; \
	./$(EXE) \
	  > $@.out; \
	diff $(EXE).out $@.out

.PHONY: run test

endif
