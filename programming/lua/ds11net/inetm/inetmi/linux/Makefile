ifndef CPULIST
CPULIST := $(shell uname -p)
endif

$(MAKECMDGOALS):
	$(foreach di, $(CPULIST), $(MAKE) -C $(di) $@;)

