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
NAME := luanet
TARGET := $(NAME).$(VAR)

INSTDIR := $(DS11INSTALL_ROOT)/usr
INCS = -I$(INSTDIR)/include

SRCS = $(wildcard ../../../*.c)
SRCS += $(wildcard ../../*.c)
SRCS += $(wildcard ../*.c)
OBJS = $(notdir $(SRCS:.c=.o))

include $(DS11ENV_ROOT)/$(CPU).toolset.mk

ifeq ($(VAR),so)
CFLAGS = -g -Wall -fPIC
LDFLAGS = -g -Wall -shared -Wl,-soname,$(TARGET)
else
CFLAGS = -g -Wall
ARFLAGS = rcs
endif

