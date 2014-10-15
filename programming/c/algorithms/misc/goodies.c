
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({ \
                typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - im_offsetof(type,member) );})
 
