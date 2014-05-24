#include "niagara_module.h"
#include "cpld.h"

static volatile long cpu_lock_bits;

static
int cpu_unlock(void)
{
	clear_bit(1, &cpu_lock_bits);
	return 0;
}

static
int cpu_trylock(void)
{
	return test_and_set_bit(1, &cpu_lock_bits);
}

static
int cpu_lock(void)
{
	for (;;) {
		if (cpu_trylock() == 0)
			return 0;
		schedule();
	}
}

static
int cpu_wait(void)
{
	uint8_t stat;
	int timeout = 0;

	for (;;) {
		stat = cpld_read(CSR);
		if ((stat & 0x08) == 0)
			return 0;
		if (++timeout == 10000)
			break;
		udelay(10);
	}
	MSG("CPLD timeout!");
	return -1;
}

uint8_t cpu_read(uint8_t offset)
{
	uint8_t res;

	cpu_lock();
	cpld_write(AVR_ADDR, offset);
	cpld_write(CSR, 0x8);
	cpu_wait();
	res = cpld_read(HOST_DAT);
	cpu_unlock();
	return res;
}

uint8_t cpu_write(uint8_t offset, uint8_t value)
{
	uint8_t res;

	cpu_lock();
	cpld_write(AVR_ADDR, offset);
	cpld_write(AVR_DAT, value);
	cpld_write(CSR, 0xC);
	res = cpu_wait();
	cpu_unlock();
	return res;
}

