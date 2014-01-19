TARGET := alignment

CC := gcc
CFLAGS = -g -Wall

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.c) $^ -o $@

