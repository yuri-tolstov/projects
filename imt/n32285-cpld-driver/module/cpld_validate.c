#include "niagara_module.h"
#include "cpld.h"

unsigned char *hw_port[MAX_PORT];
unsigned char *hw_ctrl[2];

const cpld_functions_t *cpld_functions;

static
int cpld_accesible(void)
{
	cpld_init();
	cpld_write(AVR_ADDR, 0x02);
	return cpld_read(AVR_ADDR) == 0x02;
}

static
int validate_ports(unsigned char *ctrl0, unsigned char *ctrl1)
{
	hw_ctrl[0] = ctrl0;
	hw_ctrl[1] = ctrl1;
	return cpld_accesible();
}

int cpld_validate(card_t *card)
{
	int port;

	switch (card->flags & F_SIDE) {
	case F_SDP:
		cpld_functions = &cpld_functions_sdp;

		for (port = 0; port < MAX_PORT; port++) {
			hw_port[port] = ioremap(pci_resource_start(card->pci_dev[port], 0),
						pci_resource_len(card->pci_dev[port], 0));
			if (hw_port[port] == NULL)
				MSG_TTY("Failed to map port %d", port);
		}
		/* Try on Segment 1*/
		if (validate_ports(hw_port[0], hw_port[1]))
			return 0;
		/* Try on Segment 2*/
		if (validate_ports(hw_port[1], hw_port[0]))
			return 0;
		break;
	default:
		return -1;
	}
	return 1;
}

