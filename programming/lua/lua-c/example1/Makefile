LUAHOME=$(HOME)/Downloads/lua-5.2.2/src

TARGET := sampleluahost

all: $(TARGET)

$(TARGET): main.cpp
	g++ -g -Wall -o $@ $< -I$(LUAHOME) -L$(LUAHOME) -llua -ldl

clean:
	rm -f $(TARGET)
