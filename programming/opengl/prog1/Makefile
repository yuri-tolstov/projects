TARGET := prog1

CFLAGS = -g -Wall
LDFLAGS = -lglut

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.c
	$(LINK.cc) $^ -o $@

