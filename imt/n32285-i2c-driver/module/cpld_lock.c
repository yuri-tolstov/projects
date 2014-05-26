#include "niagara_module.h"

static volatile long cpld_lock_bits;

int cpld_unlock(void)
{
	clear_bit(1, &cpld_lock_bits);
	return 0;
}

int cpld_trylock(void)
{
	return test_and_set_bit(1, &cpld_lock_bits);
}

int cpld_lock(void)
{
	for (;; ) {
		if (cpld_trylock() == 0)
			return 0;
		if (in_atomic())
			return -1;
		schedule();
	}
}

