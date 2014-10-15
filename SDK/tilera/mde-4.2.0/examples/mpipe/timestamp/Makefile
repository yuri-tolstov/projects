progs = app testptp

all: $(progs)

ifdef TILERA_ROOT

CC = $(TILERA_ROOT)/bin/tile-gcc
MPIPE_CC = $(TILERA_ROOT)/bin/tile-mpipe-cc

else

ifeq ($(filter tile%,$(shell uname -m)),)
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = gcc
MPIPE_CC = tile-mpipe-cc

endif

OPT = -Os
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT)
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static -pthread)
LIBS = -lgxio -ltmc

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app: app.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< $(LIBS)

testptp: testptp.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $< -lrt

clean:
	rm -f *.o $(progs)

.PHONY: all clean
