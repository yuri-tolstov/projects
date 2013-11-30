#-------------------------------------------------------------------------------
# This file is included from the lowest level Makefile:
# $(OS)/$(CPU)/$(VAR)/Makefile
#-------------------------------------------------------------------------------
.PHONY: all install clean

#-------------------------------------------------------------------------------
# Explicit Rules
#-------------------------------------------------------------------------------
all: $(TARGET)

$(NAME).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(NAME).so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

install: $(TARGET)
	mkdir -p $(INSTDIR)/include
	mkdir -p $(INSTDIR)/$(CPU)/lib
	cp $(TARGET) $(INSTDIR)/$(CPU)/lib

clean:
	rm -f $(TARGET) $(OBJS)

distclean: clean
	rm -f $(INSTDIR)/$(CPU)/lib/$(TARGET)

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
$(OBJS): ../../../comdefs.mk

$(OBJS): ../../../comrules.mk

