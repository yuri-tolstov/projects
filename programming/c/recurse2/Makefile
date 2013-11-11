TARGET := reverse
CFLAGS = -g -Wall

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.cc) $^ -o $@

