obj-m = wqexample1.o

KVERSION := $(shell uname -r)
KSOURCE := /lib/modules/$(KVERSION)/build

all:
	make -C $(KSOURCE) M=`pwd` modules

clean:
	make -C $(KSOURCE) M=`pwd` clean
