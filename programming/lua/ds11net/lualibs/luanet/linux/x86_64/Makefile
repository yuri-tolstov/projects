VARLIST := so

$(MAKECMDGOALS):
	$(foreach di, $(VARLIST), $(MAKE) -C $(di) $@;)

