TARGET := netbm-pcap
#-------------------------------------------------------------------------------
CC = gcc

INCS += -I../libpcap/usr/include

CFLAGS += -g -O0 $(INCS)
LDFLAGS += -g -O0
# LDLIBS += -L../libpcap/usr/lib64 -lpcap
LDLIBS += -lpcap

OBJS = main.o

#-------------------------------------------------------------------------------
.PHONY: all clean
all: $(TARGET) ;

$(TARGET): $(OBJS)
	$(LINK.o) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

