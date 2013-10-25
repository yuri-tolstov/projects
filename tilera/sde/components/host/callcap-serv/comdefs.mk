#-------------------------------------------------------------------------------
# Check the environment
#-------------------------------------------------------------------------------
ifdef DS11ENV_ROOT
include $(DS11ENV_ROOT)/envcheck.mk
else
$(error "DS11ENV_ROOT is not defined")
endif
#-------------------------------------------------------------------------------
# This file is included from the lowest level Makefile:
# $(OS)/$(CPU)/$(VAR)/Makefile
#-------------------------------------------------------------------------------
NAME := callcap-serv
TARGET := $(NAME)

INSTDIR := $(DS11INSTALL_ROOT)/usr
CDEFS = -DTILEPCI_HOST
INCS = -I$(INSTDIR)/include -I$(DS11VIEW_ROOT)/ds11.bin/usr/include -I$(TILERA_ROOT)/include
# LIBLPATHS = -L$(INSTDIR)/$(CPU)/lib -L$(DS11VIEW_ROOT)/ds11.bin/usr/$(CPU)/lib
LIBLPATHS = -L$(INSTDIR)/$(CPU)/lib -L$(DS11VIEW_ROOT)/ds11.bin/usr/$(CPU)/lib -L$(TILERA_ROOT)/lib

SRCS = $(wildcard ../../../*.c)
SRCS += $(wildcard ../../*.c)
SRCS += $(wildcard ../*.c)
OBJS = $(notdir $(SRCS:.c=.o))

include $(DS11ENV_ROOT)/$(CPU).toolset.mk

CFLAGS += -std=gnu99 -g -Wall $(CDEFS) $(INCS)
LDFLAGS += -g -Wall
LDLIBS = -lpthread
# LDLIBS = $(LIBLPATHS) -lgxpci -lgxio -ltmc

