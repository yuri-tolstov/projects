TARGET := prog

SRCS := main.c
OBJS = $(SRCS:%.c=%.o)

CC = gcc
CFLAGS = -g -Wall

.PHONY: all clean

all: $(TARGET) ;

$(TARGET): $(OBJS)
	$(LINK.c) -o $@ $^

clean:
	rm -rf $(TARGET) *.o

