COMLIST := luanet

.PHONY: all
ifeq ($(MAKECMDGOALS),)
install:
else
$(MAKECMDGOALS):
endif
	$(foreach di, $(COMLIST), $(MAKE) -C $(di) $@;)

