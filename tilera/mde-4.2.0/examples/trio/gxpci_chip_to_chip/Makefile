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
CFLAGS = -Wl,--fatal-warnings -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static)
LIBS = -lgxpci -lgxio -ltmc -pthread

c2c.o: c2c.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

c2c: c2c.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

all: c2c

clean:
	rm -f *.o c2c

.PHONY: all clean

ifdef TILERA_ROOT

test_dev: all
	echo "WARNING: This example cannot be tested on hardware."

test_sim: all
	echo "WARNING: This example cannot be tested on the simulator."

.PHONY: test_dev test_sim

endif
