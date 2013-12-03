LUAHOME=$(HOME)/Downloads/lua-5.2.2/src

TARGET := example
INCS = -I$(LUAHOME)
LDPATH = -L$(LUAHOME)

all: $(TARGET)

$(TARGET): main.c
	gcc -g -Wall -o $@ $< $(INCS) $(LDPATH) -llua -ldl -lm

clean:
	rm -f $(TARGET)
