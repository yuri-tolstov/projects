#include <stdio.h>
#include <string.h>

typedef struct slink_s {
   struct slink_s *next;
   struct slink_s *prev;
} slink_t;

typedef struct dlink_s {
   struct dlink_s *next;
   struct dlink_s *prev;
} dlink_t;

typedef struct slnode_s {
   slink_t link;
   int val;
} slnode_t;

typedef struct dlnode_s {
   dlink_t link;
   int val;
} dlnode_t;

dlink_t *lhead;

/******************************************************************************/
int main(void)
/******************************************************************************/
{
   dlnode_t *node;

   for (i = 0; i < 256; i++) {
      node = malloc(sizeof(dlnode_t); 
      ...
   }





	return 0;
}

#if 0
/*----------------------------------------------------------------------------*/
/* Alignment.                                                                 */
/*----------------------------------------------------------------------------*/
#define ALIGN(p, s) (p + (s - 1)) & ~(s - 1)
#define ISALIGNED (p, s) ((p & (s - 1)) == 0)

char *mb = malloc(N);
char *pb = ALIGN(mb, 32);

/*----------------------------------------------------------------------------*/
/* Array length.                                                              */
/*----------------------------------------------------------------------------*/
#define ARRLEN(a) (sizeof(a) / sizeof(a[0]))

/*----------------------------------------------------------------------------*/
/* Field offset.                                                              */
/*----------------------------------------------------------------------------*/
#define OFFS(sname, smem) &(sname *)NULL->smem)
#endif
