VARLIST := o

$(MAKECMDGOALS):
	$(foreach di, $(VARLIST), $(MAKE) -C $(di) $@;)

