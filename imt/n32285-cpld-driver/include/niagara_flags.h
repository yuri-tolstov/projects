/* Interface to CPLD */
#define F_SIDE          0x0F00
#define F_SDP           0x0200

/* Intel driver */
#define F_DRIVER        0xF000
#define F_IGB           0x2000

/* MAC device */
#define F_CHIP		0xF0000
#define F_82576EB	0x30000
#define MAC_82576EB     (F_82576EB | F_SDP | F_IGB)

static inline
const char *flag2side(unsigned flag)
{
	switch (flag & F_SIDE) {
	case F_SDP: return "SDP";
	default: return "XXX";
	}
}

static inline
const char *flag2driver(unsigned flag)
{
	switch (flag & F_DRIVER) {
	case F_IGB: return "IGB";
	default: return "XXX";
	}
}

#define flag2str(f) ({ \
	static char _str[256]; \
	snprintf(_str, sizeof(_str), "%s %s", \
                 flag2driver(f), flag2side(f)); _str; \
})

