OSLIST := linux

.PHONY: all
ifeq ($(MAKECMDGOALS),)
all:
else
$(MAKECMDGOALS):
endif
	$(foreach di, $(OSLIST), $(MAKE) -C $(di) $@;)

