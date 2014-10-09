/******************************************************************************/
/* Non-circular single-linked list.                                           */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>

/******************************************************************************/
/* Implemnentaiton 1.                                                         */
/******************************************************************************/
#if 0
struct sl_node_s {
   void *next;
   int key;
   int data;
} sl_node_t;

sl_node_t *head;
sl_node_t *tail;

void list_add_head(sl_node_t *new, key)
{
   new->key = key;

   if (head == NULL)
      head = tail = new;
   else {
      new->next = head;
      head = new;
   }
}

void list_del(int key)
{
   sl_node_t *pp, *pc;

   for (pp = NULL, pc = head; pc != tail; pp = pc, pc = pc->next) {
      if (pc->key == key) {
         if (pc == head) {
            if (pc == tail)
               head = tail = NULL;
            else
               head = pc->next;
         }
         else {
            if (pc == tail)
               tail = pp;
         }
      }
   }   
}
#endif
/******************************************************************************/
/* Implemnentaiton 2.                                                         */
/******************************************************************************/
#if 1
struct link_s {
   struct link_s *next;
};

struct link_s *head;
struct link_s *tail;

struct node_s {
   struct link_s *next;
   int key;
   int data;
};

void list_add(struct node_s *new)
{
}

void list_del(int key)
{
}

struct node_s* list_find(int key)
{
   struct link_s *pc, *pp;

   if (head == NULL)
      return NULL;

   for (pp = NULL, pc = head; pc != tail; pp = pc, pc = pc->next) {
      if (((struct node_s *)pc)->key == key)
         return container_of(pc, struct node_s, link);
   }
   return NULL;
}
#endif

/******************************************************************************/
int main(void)
/******************************************************************************/
{

   return 0;
}

