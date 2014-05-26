MODNAME := niagara
DRVNAME := n33285-cpld-driver

VERSION_FILE := include/niagara_api.h
VERSION_MAJOR := $(shell awk '$$2 ~ /MAJOR/ {print $$3}' $(VERSION_FILE))
VERSION_MINOR := $(shell awk '$$2 ~ /MINOR/ {print $$3}' $(VERSION_FILE))
VERSION_BUILD := $(shell awk '$$2 ~ /BUILD/ {print $$3}' $(VERSION_FILE))
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR)-$(VERSION_BUILD)

.PHONY: clean install insmod rmmod

all: module/$(MODNAME).ko

module/$(MODNAME).ko: include/niagara_config.h
	make -C module modules

clean:
	make -C module clean

install: 
	make -C module modules_install
	install -m 0755 usr/bin/n511reset /usr/local/bin
	install -m 0644 udev/40-niagara.rules /lib/udev/rules.d
	depmod -a

uninstall:
	rm -f /lib/udev/rules.d/40-niagara.rules
	rm -rf /lib/modules/`uname -r`/extra/$(MODNAME).ko
	rm -rf /usr/local/bin/n511reset
	depmod -a

insmod: module/$(MODNAME).ko
	insmod $<

rmmod:
	rmmod $(MODNAME)

