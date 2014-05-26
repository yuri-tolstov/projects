#ifndef NIAGARA_MODULE_H
#define NIAGARA_MODULE_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/netdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <asm/io.h>

#include "niagara_config.h"
#include "niagara_api.h"
#include "niagara_flags.h"

#define STR(__x) #__x
#define INT2STR(__x) STR(__x)
#define NIAGARA_VERSION_S INT2STR(NIAGARA_VERSION_MAJOR) "." \
                          INT2STR(NIAGARA_VERSION_MINOR) "-" \
                          INT2STR(NIAGARA_VERSION_BUILD)

typedef struct {
	const char *name;
	int vendor, device, ss_vendor, ss_device;
	uint32_t flags;
} pci_card_t;

typedef struct {
	struct pci_dev *pci_dev[MAX_PORT];
	struct net_device *net_dev[MAX_PORT];
	unsigned char cpld_id;
	unsigned char oem_id;
	unsigned char product_id;
	unsigned char product_rev;
	unsigned char firmware_rev;
	unsigned char secondary_firmware_rev;
	char name[20];
	uint32_t flags;
} card_t;

static inline
const char *_basename(const char *arg)
{
	const char *res = arg;

	while (*arg)
		if (*arg++ == '/')
			res = arg;
	return res;
}

#define DBG(fmt, args ...) printk(KERN_ERR "niagara %s:%d:%s:"fmt "\n",\
                                  _basename(__FILE__), __LINE__, __FUNCTION__, ## args)
#define MSG(fmt, args ...) printk(KERN_INFO "niagara "fmt "\n", ## args)

static inline
void print_tty(const char *str, int len)
{
	struct tty_struct *tty = current->signal->tty;

	MSG("%s", str);
	if (tty == NULL)
		return;
	tty->driver->ops->write(tty, str, len);
	tty->driver->ops->write(tty, "\r\n", 2);
}

#define MSG_TTY(fmt, args ...) \
do { \
	char _str[256]; \
	int _len = scnprintf(_str, sizeof(_str), fmt, ## args); print_tty(_str, _len); \
} while (0)

#define itoa(x) ({char _str[64]; snprintf(_str, sizeof(_str), "%X", x); _str; })
#define atoi(x) simple_strtoul(x, NULL, 16)

#include <linux/hardirq.h>

static inline
void msec_delay(int x)
{
	if (in_interrupt()) {
		while (x > 0) {
			udelay(1000);
			x--;
		}
	} else {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout((x * HZ + 999) / 1000);
	}
}

extern pci_card_t pci_card;
extern card_t card;

typedef struct {
	void (*cpld_init)(void);
	uint8_t (*cpld_read)(uint8_t addr);
	int (*cpld_write)(uint8_t addr, uint8_t data);
} cpld_functions_t;

int cpld_lock(void);
int cpld_unlock(void);
int cpld_trylock(void);

extern cpld_functions_t cpld_functions_sdp;
extern const cpld_functions_t *cpld_functions;
extern unsigned char *hw_port[MAX_PORT];
extern unsigned char *hw_ctrl[2];
int cpld_validate(card_t *card);

#define cpld_init()           cpld_functions->cpld_init()
#define cpld_read(args ...)   cpld_functions->cpld_read(args)
#define cpld_write(args ...)  cpld_functions->cpld_write(args)

uint8_t cpu_read(uint8_t offset);
uint8_t cpu_write(uint8_t offset, uint8_t value);

int sysfs_populate(struct device *device);

int init_cpld_dev(struct kobject *kobj_parent);
void delete_cpld_dev(void);

extern struct i2c_adapter i2c_adap;
const extern struct i2c_algorithm i2c_cpld_algo;

#endif // NIAGARA_MODULE_H

