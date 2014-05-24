#include "niagara_module.h"
#include "cpld.h"
#include "cpu.h"

pci_card_t pci_card = {
	/* Name    VID     DID     SVID    SDID    MAC */
	"N32285", 0x8086, 0x10E6, 0x1FC1, 0xFFFE, MAC_82576EB
};

card_t card;
static int niagara_major;
static struct class *niagara_class;

static
int __init niagara_init(void)
{
	int rc, port;
	dev_t dev = 0;
	struct pci_dev *pci_dev = NULL;
	struct net_device *net_dev;
	struct device *device;

	for (port = 0; port < MAX_PORT; port++) {
		pci_dev = pci_get_subsys(pci_card.vendor, pci_card.device,
					 pci_card.ss_vendor,
					 pci_card.ss_device, pci_dev);
		if (pci_dev == NULL)
			return -ENXIO;

		card.pci_dev[port] = pci_dev;

		for_each_netdev(&init_net, net_dev)
			if (net_dev->dev.parent == &pci_dev->dev)
				card.net_dev[port] = net_dev;

		if (card.net_dev[port] == NULL)
			return -ENXIO;
	}
	MSG("Found %s card", pci_card.name);

	card.flags = pci_card.flags;
	snprintf(card.name, sizeof(card.name), "%s", pci_card.name);

	if (cpld_validate(&card)) {
		MSG_TTY("Cannot get response from CPLD of %s", card.name);
		return -ENXIO;
	}
	card.cpld_id = 0; /*The CPLD doesn't provide the ID register.*/
	card.oem_id = cpu_read(OEM_ID_REG);
	card.product_id = cpu_read(PRODUCT_ID_REG);
	card.product_rev = cpu_read(PRODUCT_REV_REG);
	card.firmware_rev = cpu_read(FW_ID_REG);
	card.secondary_firmware_rev = cpu_read(69);

	rc = alloc_chrdev_region(&dev, 0, 1, "niagara");
	if (rc)
		return rc;

	niagara_major = MAJOR(dev);
	niagara_class = class_create(THIS_MODULE, "niagara");

	if (IS_ERR(niagara_class))
		return PTR_ERR(niagara_class);

	device = device_create(niagara_class, NULL, MKDEV(niagara_major, 0), &card, "niagara%d", 0);
	if (IS_ERR(device))
		return PTR_ERR(device);

	rc = sysfs_populate(device);
	if (rc)
		return rc;

	if (card.pci_dev[0]->dev.driver)
		try_module_get(card.pci_dev[0]->dev.driver->owner);
	MSG("%s driver installed", pci_card.name);
	return 0;
}

static
void __exit niagara_destroy(void)
{
	device_destroy(niagara_class, MKDEV(niagara_major, 0));
	class_destroy(niagara_class);
	unregister_chrdev_region(MKDEV(niagara_major, 0), 1);

	if (card.pci_dev[0]->dev.driver)
		module_put(card.pci_dev[0]->dev.driver->owner);
	MSG("%s driver unloaded", pci_card.name);
}

module_init(niagara_init);
module_exit(niagara_destroy);

MODULE_AUTHOR("yurit@interfacemasters.com");
MODULE_LICENSE("GPL");
MODULE_VERSION(NIAGARA_VERSION_S);
MODULE_DESCRIPTION("Interface Masters, Niagara 32285 driver");

