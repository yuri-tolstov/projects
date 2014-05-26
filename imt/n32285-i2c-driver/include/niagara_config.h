#define MAX_CARD                1
#define MAX_PORT                2

#ifdef __SIZEOF_LONG__
#if (MAX_CARD > 8 * __SIZEOF_LONG__)
#error Too big MAX_CARD
#endif
#endif

