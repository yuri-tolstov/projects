TARGET := dtrctl

CC := gcc
CFLAGS = -g -O0 -Wall

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.c) $^ -o $@

