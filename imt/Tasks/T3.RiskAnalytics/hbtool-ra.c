/***
 * RiskAnalytics Modification to hbtool to work locally, instead of via pci bus
 *
 **/

#include <getopt.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "cvmx-warn.h"
#include "cvmx.h"
//#include "octeon-remote.h"
//#include "oct-remote-common.h"
#include "cvmx-mio-defs.h"
#include "cvmx-platform.h"
#include "cvmx-clock.h"
#include "cvmx-coremask.h"

/*inline uint64_t cvmx_get_cycle(void)
{
    return cvmx_clock_get_count(CVMX_CLOCK_CORE);
}
*/

#include "cvmx-mdio.h"

#define HBTOOL_SEG_NUM 2

#define HBTOOL_MDIO_BUS 0
#define HBTOOL_PHY 0x10
#define HBTOOL_AVR_ADDR 0x10
#define HBTOOL_AVR_CSR 0x17
#define HBTOOL_AVR_DIN 0x12
#define HBTOOL_AVR_DOUT 0x11
#define HBTOOL_AVR_CSR_OP_REQ (1 << 3)
#define HBTOOL_AVR_CSR_DIR_IN (0 << 2)
#define HBTOOL_AVR_CSR_DIR_OUT (1 << 2)
#define HBTOOL_AVR_CSR_KICK_A (1 << 4)
#define HBTOOL_AVR_CSR_KICK_B (1 << 5)

typedef enum {HBTOOL_MODE_GET, HBTOOL_MODE_SET, HBTOOL_MODE_LOOP, HBTOOL_MODE_LAST} hbtool_mode_t;
#define HBTOOL_OPT_SET HBTOOL_MODE_LAST
#define HBTOOL_OPT_ONE_SHOT (HBTOOL_MODE_LAST + 1)

#define HBTOOL_FLAG(x) (1 << (x))

static int get_func(const struct option *opts, size_t size, int seg, int verb);
static int set_func(const struct option *opts, size_t size, int seg, int verb);
static int loop_func(const struct option *opts, size_t size, int seg, int verb);

int hbtool_init(int verb)
{
	/*if( octeon_remote_open("PCI", verb))
	{
		printf("*** Couldn't establish connection over PCI!\n");
		return -1;
	}*/

	cvmx_write_csr(CVMX_SMIX_EN(HBTOOL_MDIO_BUS), 0x1);

	return 0;
}

static uint8_t hbtool_avr_read(uint8_t avr_reg)
{
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_ADDR, avr_reg);
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_CSR, HBTOOL_AVR_CSR_OP_REQ | HBTOOL_AVR_CSR_DIR_IN);

	while (cvmx_mdio_read(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_CSR) & HBTOOL_AVR_CSR_OP_REQ)
		usleep(100);

	return (uint8_t)cvmx_mdio_read(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_DIN);
}

static void hbtool_avr_write(uint8_t avr_reg, uint8_t data)
{
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_ADDR, avr_reg);
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_DOUT, data);
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_CSR, HBTOOL_AVR_CSR_OP_REQ | HBTOOL_AVR_CSR_DIR_OUT);

	while (cvmx_mdio_read(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_CSR) & HBTOOL_AVR_CSR_OP_REQ)
		usleep(100);
}

static void hbtool_avr_kick_heartbeat(int seg)
{
	static uint8_t seg_kicks[] = {HBTOOL_AVR_CSR_KICK_A | HBTOOL_AVR_CSR_KICK_B, HBTOOL_AVR_CSR_KICK_A, HBTOOL_AVR_CSR_KICK_B};
	cvmx_mdio_write(HBTOOL_MDIO_BUS, HBTOOL_PHY, HBTOOL_AVR_CSR, seg_kicks[seg]);
}
/**************************************************************************************/

int timeout_get(int seg)
{
	static uint8_t seg_regs[] = {0x0, 0x1};
	uint8_t m = hbtool_avr_read(seg_regs[seg - 1]);
	printf("Heartbeat timeout [%d]: %d msec\n", seg, (int) m * 100);
	return 0;
}

int timeout_set(int seg, char *v)
{
	static uint8_t seg_regs[] = {0x0, 0x1};
	int i = atoi(v); i /= 100;
	if (i <= 0xff)
	{
		hbtool_avr_write(seg_regs[seg - 1], (uint8_t)i);
		return 0;
	}
	return -1;
}

int startup_wait_get(int seg)
{
	uint8_t m = hbtool_avr_read(0x4);
	printf("Start up wait: %d sec\n", (int) m * 2);
	return 0;
}

int startup_wait_set(int seg, char *v)
{
	int i = atoi(v); i /= 2;
	if (i <= 0xff)
	{
		hbtool_avr_write(0x4, (uint8_t)i);
		return 0;
	}
	return -1;
}

int startup_wait_ovr_get(int seg)
{
	uint8_t m = hbtool_avr_read(0x5);
	printf("Start up wait override: %d\n", m);
	return 0;
}

int startup_wait_ovr_set(int seg, char *v)
{
	uint8_t i = atoi(v);
	if (i < 2)
	{
		hbtool_avr_write(0x5, i);
		return 0;
	}
	return -1;
}

int default_mode_get(int seg)
{
	static uint8_t seg_regs[] = {0x32, 0x38};
	uint8_t m = hbtool_avr_read(seg_regs[seg - 1]);
	printf("Default mode [%d]: %d\n", seg, (int) m);
	return 0;
}

int default_mode_set(int seg, char *v)
{
	static uint8_t seg_regs[] = {0x32, 0x38};
	uint8_t i = atoi(v);
	if (i < 6)
	{
		hbtool_avr_write(seg_regs[seg - 1], i);
		return 0;
	}
	return -1;
}

int poweroff_mode_get(int seg)
{
	static uint8_t seg_regs[] = {0x33, 0x39};
	uint8_t m = hbtool_avr_read(seg_regs[seg - 1]);
	printf("Power Off mode [%d]: %s\n", seg, (m)?"Bypass":"No Link");
	return 0;
}

int poweroff_mode_set(int seg, char *v)
{
	static uint8_t seg_regs[] = {0x33, 0x39};
	uint8_t i = atoi(v);
	if (i < 2)
	{
		hbtool_avr_write(seg_regs[seg - 1], i);
		return 0;
	}
	return -1;
}

int current_mode_get(int seg)
{
	static uint8_t seg_regs[] = {0x31, 0x37};
	uint8_t m = hbtool_avr_read(seg_regs[seg - 1]);
	printf("Current mode [%d]: %d\n", seg, (int) m);
	return 0;
}

int current_mode_set(int seg, char *v)
{
	static uint8_t seg_regs[] = {0x31, 0x37};
	uint8_t i = atoi(v);
	if (i < 6)
	{
		hbtool_avr_write(seg_regs[seg - 1], i);
		return 0;
	}
	return -1;
}

int status_get(int seg)
{
	static uint8_t seg_regs[] = {0x2f, 0x35};
	static char *status[] = {"BYPASS", "INLINE", "NO LINK"};

	uint8_t s = hbtool_avr_read(seg_regs[seg - 1]);
	printf("Status [%d]: %s\n", seg, status[s - 1]);
	return 0;
}

int oem_id_get(int seg)
{
	uint8_t s = hbtool_avr_read(0x41);
	printf("OEM Id: %d\n", s);
	return 0;
}

int product_rev_get(int seg)
{
	uint8_t s = hbtool_avr_read(0x43);
	printf("Product revision: %d\n", s);
	return 0;
}

int product_id_get(int seg)
{
	uint8_t s = hbtool_avr_read(0x42);
	printf("Product Id: %d\n", s);
	return 0;
}

int fw_rev_get(int seg)
{
	uint8_t s = hbtool_avr_read(0x44);
	uint8_t ss = hbtool_avr_read(0x45);
	printf("Firmware revision: %d.%d\n", s, ss);
	return 0;
}

static struct hbtool_opt
{
	char *opt;
	int has_arg;
	char *val;
	char *help;
	uint32_t flags;
	int (*set)(int seg, char *v);
	int (*get)(int seg);
} hbtool_opt_list[] = {
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET) | HBTOOL_FLAG(HBTOOL_MODE_LOOP),
		.opt = "segment",
		.has_arg =  required_argument,
		.help = "Bypass segment. If not specified or 0, retrieve or set parameters for all the segments",
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET) | HBTOOL_FLAG(HBTOOL_MODE_LOOP),
		.opt = "verbose",
		.has_arg =  no_argument,
		.help = "Verbose mode",
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "all",
		.has_arg =  no_argument,
		.help = "Retrieve all parameters",
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET),
		.opt = "timeout",
		.help = "Heartbeat timeout in units of msec",
		.has_arg = optional_argument,
		.set = timeout_set,
		.get = timeout_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "startup-wait",
		.help = "Start up wait in units of seconds",
		.has_arg = optional_argument,
		.set = startup_wait_set,
		.get = startup_wait_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "startup-wait-ovr",
		.help = "If set to 'true', the start up wait is aborted after two heartbeats are received",
		.has_arg = optional_argument,
		.set = startup_wait_ovr_set,
		.get = startup_wait_ovr_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET),
		.opt = "default-mode",
		.help = "The following default operational modes are available with respect to Heartbeat Present/No Heartbeat pair:\n\t\t0 - Active/Bypass\n\t\t1 - Bypass/Active\n\t\t2 - Active/Active\n\t\t3 - Active/No Link\n\t\t4 - Bypass/Bypass\n\t\t5 - No Link/No Link",
		.has_arg = optional_argument,
		.set = default_mode_set,
		.get = default_mode_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET),
		.opt = "poweroff-mode",
		.help = "Power OFF mode: 0 - No Link, 1 - Bypass",
		.has_arg = optional_argument,
		.set = poweroff_mode_set,
		.get = poweroff_mode_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_MODE_SET),
		.opt = "current-mode",
		.help = "Control the current operational mode upon power up.\n\t\tThis settings will not be remembered after the system is powered down.\n\t\tSee '--default-mode' option for the list of available operational modes",
		.has_arg = optional_argument,
		.set = current_mode_set,
		.get = current_mode_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET),
		.opt = "status",
		.help = "System status",
		.has_arg =  no_argument,
		.get = status_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "oem-id",
		.help = "OEM ID",
		.has_arg =  no_argument,
		.get = oem_id_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "product-id",
		.help = "Product ID",
		.has_arg =  no_argument,
		.get = product_id_get,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "product-rev",
		.help = "Product revision",
		.has_arg =  no_argument,
		.get = product_rev_get
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_GET) | HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT),
		.opt = "fw-rev",
		.help = "Firmware revision",
		.has_arg =  no_argument,
		.get = fw_rev_get
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_LOOP),
		.opt = "timeout",
		.help = "Keepalive timeout, default 100 msec",
		.has_arg =  required_argument,
	},
	{
		.flags = HBTOOL_FLAG(HBTOOL_MODE_LOOP),
		.opt = "count",
		.help = "Keepalive count, default 0 -- infinite loop",
		.has_arg =  required_argument,
	},
};

static size_t hbtool_opt_list_size = sizeof(hbtool_opt_list)/sizeof(hbtool_opt_list[0]);

static struct hbtool_mode
{
	char *mode;
	char *help;
	struct option *opts;
	struct option *opts_c;
	size_t opts_size;
	int (* mode_func)(const struct option *opts, size_t size, int seg, int verbose);
} hbtool_mode_list[] = {
	[HBTOOL_MODE_GET]  = {
		.mode = "get",
		.help = "Retrieve parameters",
		.mode_func = get_func,
	},
	[HBTOOL_MODE_SET]  = {
		.mode = "set",
		.help = "Set parameters",
		.mode_func = set_func,
	},
	[HBTOOL_MODE_LOOP] = {
		.mode = "keepalive",
		.help = "Send keepalive heartbeat",
		.mode_func = loop_func,
	},
};

static void hbtool_mode_list_init(hbtool_mode_t m)
{
	size_t i;
	for (i = 0; i < hbtool_opt_list_size; ++i)
	{
		if (hbtool_opt_list[i].flags & HBTOOL_FLAG(m))
			++hbtool_mode_list[m].opts_size;
	}

	hbtool_mode_list[m].opts = hbtool_mode_list[m].opts_c =
		(struct option *) calloc(hbtool_mode_list[m].opts_size + 1, sizeof(struct option));

	for (i = 0; i < hbtool_opt_list_size; ++i)
	{
		if (hbtool_opt_list[i].flags & HBTOOL_FLAG(m))
		{
			hbtool_mode_list[m].opts_c->name = hbtool_opt_list[i].opt;

			hbtool_mode_list[m].opts_c->has_arg = (hbtool_opt_list[i].has_arg == optional_argument) ? 
				((m == HBTOOL_MODE_GET) ? no_argument : required_argument) : hbtool_opt_list[i].has_arg;

			hbtool_mode_list[m].opts_c->flag = NULL;
			hbtool_mode_list[m].opts_c->val = i;
			hbtool_mode_list[m].opts_c++;
		}
	}
}

static hbtool_mode_t select_mode(char *arg)
{
	int i;

	if (arg)
		for (i = 0; i < HBTOOL_MODE_LAST; ++i)
			if (!strcmp(arg, hbtool_mode_list[i].mode)) return i;

	return HBTOOL_MODE_LAST;
}

static inline uint32_t get_ms()
{
	struct timeval  tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int get_func(const struct option *opts, size_t size, int seg, int verb)
{
	size_t i;
	int j, n = 0, all = 0;
	uint8_t s;

	if (hbtool_opt_list[2].flags & HBTOOL_FLAG(HBTOOL_OPT_SET)) /* --all */
		all = 1;

	for (i = 0; i < size; ++i)
	{
		struct hbtool_opt *o = &hbtool_opt_list[opts[i].val];

		if (all || o->flags & HBTOOL_FLAG(HBTOOL_OPT_SET))
		{
			n++; s = seg;

			if (o->flags & HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT)) s += !s;

			for (j = s + !s; j <= s + (!s) * HBTOOL_SEG_NUM; ++j)
			{
				if (verb) printf("*** GET: segment = %d, option = %s, flags = %d\n", j, o->opt, o->flags);

				if (o->get && o->get(j) < 0) return -1;
			}
		}
	}
	return (n > 0) ? 0 : -1;
}

static int set_func(const struct option *opts, size_t size, int seg, int verb)
{
	size_t i;
	int j, n = 0;

	for (i = 0; i < size; ++i)
	{
		struct hbtool_opt *o = &hbtool_opt_list[opts[i].val];

		if (o->flags & HBTOOL_FLAG(HBTOOL_OPT_SET)) 
		{
			n++;

			if (o->flags & HBTOOL_FLAG(HBTOOL_OPT_ONE_SHOT)) seg += (!seg);

			for (j = seg + !seg; j <= seg + (!seg) * HBTOOL_SEG_NUM; ++j)
			{
				if (verb) printf("*** SET: segment = %d, option = %s, value = %s, flags = %d\n",
						j, o->opt, o->val, o->flags);

				if (o->set && o->set(j, o->val) < 0) return -1;
			}
		}
	}
	return (n > 0) ? 0 : -1;
}

static int loop_func(const struct option *opts, size_t size, int seg, int verb)
{
#define LOOP_DEFAULT_TIMEOUT	100
#define LOOP_DEFAULT_COUNT	0

	uint32_t timeout = 0, count = 0;
	size_t i;

	for (i = 0; i < size; ++i)
	{
		struct hbtool_opt *o = &hbtool_opt_list[opts[i].val];

		if (o->flags & HBTOOL_FLAG(HBTOOL_OPT_SET)) 
		{
			if (!strcmp(opts[i].name, "timeout"))
				timeout = atoi(hbtool_opt_list[opts[i].val].val);

			if (!strcmp(opts[i].name, "count"))
				count = atoi(hbtool_opt_list[opts[i].val].val);
		}
	}

	timeout = (timeout) ? : LOOP_DEFAULT_TIMEOUT;
	count   = (count)   ? : LOOP_DEFAULT_COUNT;

	for (i = count; !count || i; i -= (!(!count)))
	{
		uint32_t start, delta;

		start = get_ms();

		hbtool_avr_kick_heartbeat(seg);

		delta = get_ms() - start; 
		if (delta > timeout)
		{
			printf("oops... it looks like I can't support this timeout\n");
			exit(1);
		}
		
		usleep((timeout - delta)*1000);
	}

	return 0;
}

void usage(const char *prog, hbtool_mode_t m)
{
	size_t i;

	switch (m)
	{
	case HBTOOL_MODE_GET:
	case HBTOOL_MODE_SET:
	case HBTOOL_MODE_LOOP:
		printf("Mode '%s': %s\n", hbtool_mode_list[m].mode, hbtool_mode_list[m].help);
		for (i = 0; i < hbtool_mode_list[m].opts_size; ++i)
		{
			int idx = hbtool_mode_list[m].opts[i].val;
			printf("\t--%s: %s\n", hbtool_opt_list[idx].opt, hbtool_opt_list[idx].help);
		}
		break;
	case HBTOOL_MODE_LAST:
		printf("Usage: %s MODE OPTION [OPTION] ...\n", prog);
		for (i = 0; i < HBTOOL_MODE_LAST; ++i)
			usage(prog, i);
		break;
	}
}

int main(int argc, char *argv[])
{
	int next_option;
	int segment = 0, verbose = 0;

	cvmx_user_app_init();
	if (!cvmx_coremask_first_core(cvmx_sysinfo_get()->core_mask)) {
              // This is not an error, but is pretty cool. When you call ra-skycoalutil
              // or a skycoal-util function say "st_rebuild" that process is run on all
              // cores available to it. In our case 2 are allocated to the kernel, which
              // means 2 are available to the simple executive userspace processes. We
              // want one instnace running, so this check guarantees we're only running
              // on one core -- the first core available to it.
              return 0;
	}

	hbtool_mode_t mode = select_mode(argv[1]);
	if (mode == HBTOOL_MODE_LAST)
	{
		usage(argv[0], mode);
		return 1;
	}

	hbtool_mode_list_init(mode);

	struct hbtool_mode *sm = &hbtool_mode_list[mode];

	do {
		next_option = getopt_long(argc - 1, argv + 1, "", sm->opts, NULL);
		if (next_option == '?')
		{
			usage(argv[0], mode);
			return 1;
		}
		if (next_option == -1)
		{
			continue;
		}

		if (optarg)
			hbtool_opt_list[next_option].val = strdup(optarg);

		hbtool_opt_list[next_option].flags |= HBTOOL_FLAG(HBTOOL_OPT_SET);
	} while (next_option != -1);

	if (hbtool_opt_list[0].flags & HBTOOL_FLAG(HBTOOL_OPT_SET))
		segment = atoi(hbtool_opt_list[0].val);

	if (hbtool_opt_list[1].flags & HBTOOL_FLAG(HBTOOL_OPT_SET))
		verbose = 1;

	if (hbtool_init(verbose) < 0) return 1;

	if (sm->mode_func && sm->mode_func(sm->opts + 2, sm->opts_size - 2, segment, verbose) < 0)
	{
		usage(argv[0], mode);
		return 1;
	}

	return 0;
}
 
