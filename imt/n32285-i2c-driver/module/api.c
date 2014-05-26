#include "niagara_module.h"
#include "cpld.h"

static
int get_version(unsigned *value)
{
	*value = NIAGARA_VERSION;
	return 0;
}

#define NIAGARA_CARD_ATTRIBUTE(_name, _mode) \
	static int get_ ## _name(unsigned *value) { \
		*value = card._name; \
		return 0; \
	}
#include "niagara_card_attributes.h"
#undef NIAGARA_CARD_ATTRIBUTE

#define NIAGARA_DRIVER_ATTRIBUTE(_name, _mode) {0, .name = # _name, get_ ## _name, NULL},
#define NIAGARA_CARD_ATTRIBUTE(_name, _mode)   {1, .name = # _name, get_ ## _name, NULL},
static const struct  {
	int	depth;
	char	name [32];
	int	(*get_attr)(unsigned *value);
	int	(*set_attr)(unsigned value);
} niagara_attributes[] = {
#include "niagara_driver_attributes.h"
#include "niagara_card_attributes.h"
#undef NIAGARA_DRIVER_ATTRIBUTE
#undef NIAGARA_CARD_ATTRIBUTE
};

int NiagaraGetAttribute(const char *name, unsigned *value)
{
	int i;

	for (i = 0; i < sizeof(niagara_attributes) / sizeof(niagara_attributes[0]); i++)
		if (strcmp(name, niagara_attributes[i].name) == 0) {
			if (niagara_attributes[i].get_attr == NULL)
				return -EPERM;
			return niagara_attributes[i].get_attr(value);
		}
	return -ENOENT;
}
int NiagaraSetAttribute(const char *name, unsigned value)
{
	int i;

	for (i = 0; i < sizeof(niagara_attributes) / sizeof(niagara_attributes[0]); i++)
		if (strcmp(name, niagara_attributes[i].name) == 0) {
			if (niagara_attributes[i].set_attr == NULL)
				return -EPERM;
			return niagara_attributes[i].set_attr(value);
		}
	return -ENOENT;
}

EXPORT_SYMBOL(NiagaraGetAttribute);
EXPORT_SYMBOL(NiagaraSetAttribute);

