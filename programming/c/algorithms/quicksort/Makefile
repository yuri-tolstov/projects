TARGET := quicksort

CC := gcc -std=c99 -pedantic
CFLAGS = -g -O -Wall

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.c) $^ -o $@

