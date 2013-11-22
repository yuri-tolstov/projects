TARGET := prog

CPPFLAGS = -g -Wall
LDFLAGS = -lm -lGL -lGLU -lglut

.PHONY: all clean

all: $(TARGET) ;

clean:
	rm -f $(TARGET) *.o

$(TARGET): main.cpp
	$(LINK.cc) $^ -o $@

