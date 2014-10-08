TARGET := list

CC := gcc -std=c99 -pedantic
INCS = -I/usr/src/linux-headers-3.13.0-36-generic/include
CFLAGS = -g -O -Wall $(INCS)

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.c) $^ -o $@

