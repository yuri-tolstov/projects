# Default target.
all:

ifdef TILERA_ROOT

CC = $(TILERA_ROOT)/bin/tile-gcc
MPIPE_CC = $(TILERA_ROOT)/bin/tile-mpipe-cc
OBJDUMP = $(TILERA_ROOT)/bin/tile-objdump
SRCDIR = $(TILERA_ROOT)/src

else

ifeq ($(filter tile%,$(shell uname -m)),)
$(error The 'TILERA_ROOT' environment variable is not set.)
endif

CC = gcc
MPIPE_CC = tile-mpipe-cc
OBJDUMP = objdump
SRCDIR = /usr/tilera/src

endif

DEFAULT_CLASSIFIER = $(SRCDIR)/sys/mpipe/classifier.c

CLASSIFIER_CFLAGS = -DCUSTOM_MAC_INFO

OPT = -O3
CFLAGS = -std=gnu99 -Wall -Werror -g $(OPT) -pthread
LDFLAGS = $(if $(filter -m32, $(XFLAGS)), -static) -pthread
LIBS = -lgxio -ltmc


app.o: app.c app.h
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

reorder.o: reorder.c app.h
	$(CC) $(CFLAGS) $(XFLAGS) -c -o $@ $<

app: app.o reorder.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS)

app.objdump: app
	$(OBJDUMP) -d app > $@

custom_classifier: $(DEFAULT_CLASSIFIER)
	$(MPIPE_CC) $(CLASSIFIER_CFLAGS) -o custom_classifier $<

all: app custom_classifier

clean:
	rm -f *.o app *.objdump custom_classifier

.PHONY: all clean


ifdef TILERA_ROOT

TILE_MONITOR = $(TILERA_ROOT)/bin/tile-monitor


WORKERS ?= 8
NUM ?= 100000
LINK ?= gbe0

COMMON_ARGS = \
  --try-console-out console.out \
  --mkdir /opt/test --cd /opt/test --upload app app \
  --tile dataplane -- app -w $(WORKERS) -n $(NUM) --link $(LINK)

DEV_ARGS = --hvx dataplane=1-255 $(COMMON_ARGS)

DEVICE_ARG =
ifdef DEV_NAME
DEVICE_ARG += --dev $(DEV_NAME)
endif

run_dev: app custom_classifier
	$(TILE_MONITOR) $(DEVICE_ARG) \
	--hvd MEMPROF_TILE=1,0 --hvx dataplane=2-35 \
	--hvx TLR_NETWORK=none --upload app /app \
	--classifier custom_classifier \
	--root \
	--tile dataplane -- /app -w 6 \
	--link xgbe1 --link xgbe2 --link xgbe3 --link xgbe4

test_dev: app
	echo "WARNING: This example cannot be tested on hardware."


run_sim: app
	echo "WARNING: This example cannot be run on the simulator."

test_sim: run_sim


.PHONY: run_dev test_dev run_sim test_sim

endif
