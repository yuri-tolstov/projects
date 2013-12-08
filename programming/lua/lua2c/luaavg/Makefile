LUAHOME=$(HOME)/Downloads/lua-5.2.2/src

TARGET := luaavg
INCS = -I$(LUAHOME)
LDPATH = -L$(LUAHOME)

$(TARGET): luaavg.cpp
	g++ -g -Wall -o $@ $< $(INCS) $(LDPATH) -llua -ldl -lm

clean:
	rm -f $(TARGET)
