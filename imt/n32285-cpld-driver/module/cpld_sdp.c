#include "niagara_module.h"
#include "cpld.h"

#define BIT3  0x8
#define BIT2  0x4
#define BIT1  0x2
#define BIT0  0x1

enum bit {ADO0, ADO1, STB0, STB1, DI0, DI1};

static const struct {
	int port;
	uint32_t datareg, datamask;
	uint32_t dirreg, dirmask;
	uint32_t output;
} cpld_pins[] = {
	[ADO1] = {0, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP7_DATA, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP7_DIR, 1}, /*DS*/
	[ADO0] = {0, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP6_DATA, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP6_DIR, 1}, /*D4*/
	[STB0] = {0, NXX_CTRL,     NXX_CTRL_SWDPIN0,       NXX_CTRL,     NXX_CTRL_SWDPIO0,	1}, /*D3*/
	[STB1] = {1, NXX_CTRL,     NXX_CTRL_SWDPIN0,       NXX_CTRL,     NXX_CTRL_SWDPIO0,	1}, /*D0*/
	[DI1] =	 {1, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP7_DATA, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP7_DIR},    /*D2*/
	[DI0] =	 {1, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP6_DATA, NXX_CTRL_EXT, NXX_CTRL_EXT_SDP6_DIR},    /*D1*/
};

static
void cpld_sdp_init(void)
{
	int i;
	void *reg;
	uint32_t val;

	for (i = 0; i < sizeof(cpld_pins) / sizeof(cpld_pins[0]); i++) {
		reg = hw_ctrl[cpld_pins[i].port] + cpld_pins[i].dirreg;
		val = ioread32(reg);
		if (cpld_pins[i].output)
			val |= cpld_pins[i].dirmask;
		else
			val &= ~cpld_pins[i].dirmask;
		iowrite32(val, reg);
	}
}


static
void inline bit_up(enum bit bit)
{
	void *reg = hw_ctrl[cpld_pins[bit].port] + cpld_pins[bit].datareg;
	uint32_t val = ioread32(reg);

	val |= cpld_pins[bit].datamask;
	iowrite32(val, reg);
}

static
void inline bit_down(enum bit bit)
{
	void *reg = hw_ctrl[cpld_pins[bit].port] + cpld_pins[bit].datareg;
	uint32_t val = ioread32(reg);

	val &= ~cpld_pins[bit].datamask;
	iowrite32(val, reg);
}

static
void inline bit_set(enum bit bit, uint32_t value)
{
	if (value)
		bit_up(bit);
	else
		bit_down(bit);
}

static
int inline bit_get(enum bit bit)
{
	void *reg = hw_ctrl[cpld_pins[bit].port] + cpld_pins[bit].datareg;
	return !!(ioread32(reg) & cpld_pins[bit].datamask);
}

static
int cpld_sdp_tx_two(uint8_t addr, uint8_t bsl, uint8_t dat)
{
	bit_up(STB0);
	bit_up(STB1);
	udelay(1);

	bit_set(ADO1, addr & BIT3);
	bit_set(ADO0, addr & BIT2);
	bit_down(STB0);
	bit_down(STB1);
	udelay(1);

	bit_set(ADO1, addr & BIT1);
	bit_set(ADO0, addr & BIT0);
	bit_up(STB0);
	udelay(1);

	bit_set(ADO1, bsl & BIT1);
	bit_set(ADO0, bsl & BIT0);
	bit_down(STB0);
	udelay(1);

	bit_set(ADO1, dat & BIT1);
	bit_set(ADO0, dat & BIT0);
	bit_up(STB1);
	udelay(1);

	bit_down(STB1);
	udelay(1);
	return 0;
}

static
int cpld_sdp_rx_two(uint8_t addr, uint8_t bsl, uint8_t *val)
{
	bit_up(STB0);
	bit_up(STB1);
	udelay(1);

	bit_set(ADO1, addr & BIT3);
	bit_set(ADO0, addr & BIT2);
	bit_down(STB0);
	bit_down(STB1);
	udelay(1);

	bit_set(ADO1, addr & BIT1);
	bit_set(ADO0, addr & BIT0);
	bit_up(STB0);
	udelay(1);

	bit_set(ADO1, bsl & BIT1);
	bit_set(ADO0, bsl & BIT0);
	bit_down(STB0);
	udelay(1);

	*val = (bit_get(DI1) << 1) | bit_get(DI0);
	return 0;
}

static
uint8_t cpld_sdp_read(uint8_t addr)
{
	uint8_t value, tmp;

	cpld_lock();
	cpld_sdp_rx_two(addr, 0, &tmp);  value = tmp & 3;
	cpld_sdp_rx_two(addr, 1, &tmp);  value |= (tmp & 3) << 2;
	cpld_sdp_rx_two(addr, 2, &tmp);  value |= (tmp & 3) << 4;
	cpld_sdp_rx_two(addr, 3, &tmp);  value |= (tmp & 3) << 6;
	cpld_unlock();
	return value;
}

static
int cpld_sdp_write(uint8_t addr, uint8_t data)
{
	cpld_lock();
	cpld_sdp_tx_two(addr, 0, (data >> 0) & 3);
	cpld_sdp_tx_two(addr, 1, (data >> 2) & 3);
	cpld_sdp_tx_two(addr, 2, (data >> 4) & 3);
	cpld_sdp_tx_two(addr, 3, (data >> 6) & 3);
	cpld_unlock();
	return 0;
}

cpld_functions_t cpld_functions_sdp = {
	cpld_sdp_init,
	cpld_sdp_read,
	cpld_sdp_write
};

