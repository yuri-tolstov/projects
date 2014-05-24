#include "niagara_module.h"

#ifdef CONFIG_NIAGARA_FIRMWARE
#include <linux/firmware.h>
const struct firmware *fw;
#endif

static ssize_t cpld_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t cpld_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count);

#define NIAGARA_CPLD_ATTRIBUTE(_name, _mode) \
static struct kobj_attribute cpld_attribute_ ## _name = { \
	.attr = {.name = # _name, .mode = _mode},\
	.show = cpld_show, .store = cpld_store};
#include "niagara_cpld_attributes.h"
#undef NIAGARA_CPLD_ATTRIBUTE

static struct attribute *cpld_attrs[] = {
#define NIAGARA_CPLD_ATTRIBUTE(_name, _mode) &cpld_attribute_ ## _name.attr,
#include "niagara_cpld_attributes.h"
#undef NIAGARA_CPLD_ATTRIBUTE
	NULL,
};

static struct kobject *cpld = NULL;

static inline
unsigned get_cpld_reg_addr(struct attribute *attrp)
{
	unsigned i;
	for (i = 0; i < sizeof(cpld_attrs) / sizeof(cpld_attrs[0]); i++)
		if (cpld_attrs[i] == attrp)
			break;
	return i;
}

static const struct attribute_group sysfs_cpld_group = {
	.attrs = cpld_attrs,
};

static
ssize_t cpld_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02X\n", 0xff & cpld_read(get_cpld_reg_addr(&attr->attr)));
}

static
ssize_t cpld_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned value;
	char *eptr;
	unsigned offset;

	offset = get_cpld_reg_addr(&attr->attr);
	value = simple_strtoul(buf, &eptr, 16);

	if ((*eptr != 0) && (*eptr != '\n'))
		return -EINVAL;
	if (value >> 8)
		return -EINVAL;
	cpld_write(offset, value);
	return count;
}

int init_cpld_dev(struct kobject *kobj_parent)
{
	int rc = 0;

	if (cpld)
		return -EEXIST;

	cpld = kobject_create_and_add("cpld", kobj_parent);
	if (IS_ERR(cpld))
		return PTR_ERR(cpld);
	
	rc = sysfs_create_group(cpld, &sysfs_cpld_group);
	if (rc)
		delete_cpld_dev();
	return rc;
}

void delete_cpld_dev(void)
{
	kobject_put(cpld);
	cpld = NULL;
}

