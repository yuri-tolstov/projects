#-------------------------------------------------------------------------------
# Machine, Tools and General Stuff
#-------------------------------------------------------------------------------
CPU := $(shell uname -p)
ifeq ($(CPU),x86_64)
   CMACHOPTS = -DP64BIT
else
$(error "Not supported Machine")
endif
include $(DS11ENV_ROOT)/$(CPU).toolset.mk

#-------------------------------------------------------------------------------
# Local definitions
#-------------------------------------------------------------------------------
INSTDIR := $(DS11INSTALL_ROOT)/usr
TGTDIR := $(INSTDIR)/$(CPU)/bin
SRCDIR := $(DS11VIEW_ROOT)/ds11.net/components/host/inetm/inetme

TGTFILES := $(TGTDIR)/inetme

#-------------------------------------------------------------------------------
# Rules
#-------------------------------------------------------------------------------
.PHONY: all clean install
all:
	@echo "Done"

install: $(TGTFILES)
	@echo "Done"

clean:
	@echo "Done"

$(TGTFILES): $(TGTDIR)%: $(SRCDIR)%
	mkdir -p $(dir $@)
	cp $< $@
	@chown ditech.ditech $@

