#-------------------------------------------------------------------------------
# This file is included from the lowest level Makefile:
# $(OS)/$(CPU)/$(VAR)/Makefile
#-------------------------------------------------------------------------------
.PHONY: all install clean

#-------------------------------------------------------------------------------
# Explicit Rules
#-------------------------------------------------------------------------------
all: $(TARGET) ;

$(TARGET): $(OBJS)
	$(LINK.o)-o $@ $^ $(LDLIBS)

install: $(TARGET)
	mkdir -p $(INSTDIR)/$(CPU)/bin
	cp $(TARGET) $(INSTDIR)/$(CPU)/bin

clean:
	rm -f $(TARGET) $(OBJS)

distclean: clean
	rm -f $(INSTDIR)/$(CPU)/bin/$(TARGET)

#-------------------------------------------------------------------------------
# Implicit Rules
#-------------------------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ../%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ../../%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: ../../../%.c
	$(CC) $(CFLAGS) -o $@ -c $<

#-------------------------------------------------------------------------------
# Dependencies
#-------------------------------------------------------------------------------
# $(OBJS): $(INSTDIR)/include/xxx.h $(INSTDIR)/$(CPU)/lib/libxxx.so

$(OBJS): ../../../comdefs.mk

$(OBJS): ../../../comrules.mk

