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
CFLAGS = -Wl,--fatal-warnings -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxpci -lgxio -ltmc

bench.o: bench.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

bench: bench.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: bench

clean:
	rm -f *.o bench

.PHONY: all clean


ifdef TILERA_ROOT

test_dev: all
	echo "WARNING: This example cannot be tested on hardware."

test_sim: all
	echo "WARNING: This example cannot be tested on the simulator."

.PHONY: test_dev test_sim

endif
