#include "niagara_module.h"

#ifdef CONFIG_NIAGARA_FIRMWARE
#include <linux/firmware.h>
const struct firmware *fw;
#endif

#define DEBUG(format, args ...) \
	printk(KERN_ERR "%s:%u:%s:"format "\n", __FILE__, __LINE__, __FUNCTION__, ## args)

static
ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%06X\n", NIAGARA_VERSION);
}

static
ssize_t nxx_card_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s %s\n",
                         pci_card.name, flag2str(pci_card.flags));
}

#define NIAGARA_DRIVER_ATTRIBUTE(_name, _mode) \
	static struct kobj_attribute driver_attribute_ ## _name = { \
		.attr = {.name = # _name, .mode = _mode}, \
		.show = _name ## _show};
#include "niagara_driver_attributes.h"
#undef NIAGARA_DRIVER_ATTRIBUTE

static struct kobj_attribute driver_attribute_card = {
	.attr = {.name = "card", .mode = 0444},
	.show = nxx_card_show};

static struct attribute *driver_attrs[] = {
#define NIAGARA_DRIVER_ATTRIBUTE(_name,	_mode) & driver_attribute_ ## _name.attr,
#include "niagara_driver_attributes.h"
#undef NIAGARA_DRIVER_ATTRIBUTE
	&driver_attribute_card.attr,
	NULL,                                   /* need to NULL terminate the list of attributes */
};

static const struct attribute_group sysfs_driver_group = {
	.attrs	= driver_attrs,
};

static
ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", card.name);
}

static
ssize_t card_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc;
	unsigned value;

	rc = NiagaraGetAttribute(attr->attr.name, &value);
	if (rc)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%X\n", value);
}

#define NIAGARA_CARD_ATTRIBUTE(_field, _mode) { \
	.attr = {.name = # _field, .mode = _mode}, \
	.show = card_show},

static struct device_attribute device_attributes[] = {
	{.attr = {"name", .mode = 0444}, .show = name_show},
#include "niagara_card_attributes.h"
};
#undef NIAGARA_CARD_ATTRIBUTE

#define CPLD_MEM_SIZE 13
#define CPU_MEM_SIZE 0x7F

int __init sysfs_populate(struct device *device)
{
	int rc, a;
	int mn = MINOR(device->devt);
#ifdef CONFIG_NIAGARA_FIRMWARE
	char fwname[256];
#endif
	rc = sysfs_create_group(device->kobj.parent, &sysfs_driver_group);
	if (rc)
		return rc;

	for (a = 0; a < sizeof(device_attributes) / sizeof(device_attributes[0]); a++) {
		rc = device_create_file(device, device_attributes + a);
		if (rc)
			return rc;
	}
	rc = sysfs_create_link(device->kobj.parent, &device->kobj, itoa(mn));
	if (rc)
		return rc;

	rc = sysfs_create_link(&device->kobj, &i2c_adap.dev.kobj, "i2c");
	if (rc)
		return rc;

#ifdef CONFIG_NIAGARA_FIRMWARE
	snprintf(fwname, sizeof(fwname), "niagara_%s.bin", card.name);
	rc = request_firmware(&fw, fwname, device);
	if (rc == 0) {
		firmware_upgrade(mn, fw->data, fw->size);
		release_firmware(fw);
	} else {
		DBG("Can't find firmware %s, no upgrade", fwname);
	}
#endif
	return init_cpld_dev(&device->kobj);
}

