TARGET := hbtool

INCS = -I$(OCTEON_ROOT)/bootloader/u-boot/arch/mips/include/asm/arch-octeon
INCS += -I$(OCTEON_ROOT)/executive
INCS += -I$(OCTEON_ROOT)/host/remote-lib -I$(OCTEON_ROOT)/host/remote-utils
CC = gcc
CFLAGS = -O3 -Wall -Wno-unused-parameter \
   -DUSE_RUNTIME_MODEL_CHECKS=1 -DCVMX_BUILD_FOR_LINUX_HOST=1 $(INCS)

LD = gcc
LDFLAGS = -O3 -Wall
LIBS = $(OCTEON_ROOT)/host/remote-lib/libocteon-remote.a

SRCS = main.c n804.c n808.c
SRCS += $(OCTEON_ROOT)/executive/cvmx-warn.c $(OCTEON_ROOT)/executive/cvmx-clock.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(LINK.o) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) *.o *.tgz

