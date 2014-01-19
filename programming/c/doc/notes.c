1. Array size:
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

2. Alignment:
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))



