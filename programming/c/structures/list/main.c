#include <stdio.h>
#include <stdlib.h>
/*
http://codingfreak.blogspot.com/2012/02/implementation-of-doubly-linked-list-in.html
*/

/* Linux:
In include/linux/list.h

struct list_head {
   struct list_head *next;
   struct list_head *prev;
};
*/ 

typedef struct list_head_s {
   int len;
   struct list_head_s *first;
} list_head_t;

typedef struct list_node_s {
   void *data;
   struct list_node_s *next;
   struct list_node_s *prev;
} list_node_t;


/******************************************************************************/
int main(void)
/******************************************************************************/
{
   return 0;
}

