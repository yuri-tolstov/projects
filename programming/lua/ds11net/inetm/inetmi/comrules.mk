#-------------------------------------------------------------------------------
# This file is included from the lowest level Makefile:
# $(OS)/$(CPU)/$(VAR)/Makefile
#-------------------------------------------------------------------------------
.PHONY: all install clean

#-------------------------------------------------------------------------------
# Explicit Rules
#-------------------------------------------------------------------------------
all: $(TARGET)

lib$(NAME).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

lib$(NAME).so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

install: $(TARGET)
	mkdir -p $(INSTDIR)/include
	mkdir -p $(INSTDIR)/$(CPU)/lib
	cp $(TARGET) $(INSTDIR)/$(CPU)/lib
	cp ../../../public/inetmi.h $(INSTDIR)/include

clean:
	rm -f $(TARGET) $(OBJS)

distclean: clean
	rm -f $(INSTDIR)/$(CPU)/lib/$(TARGET)
	rm -f $(INSTDIR)/include/inetmi.h

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
$(OBJS): ../../../public/inetmi.h

$(OBJS): ../../../comdefs.mk

$(OBJS): ../../../comrules.mk

