/******************************************************************************/
/* Non-circular double-linked list.                                           */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>

/******************************************************************************/
/* Implemnentaiton 1.                                                         */
/******************************************************************************/
struct link_s {
   struct link_s *next;
   struct link_s *prev;
};

struct link_s *head;
struct link_s *tail;

struct node_s {
   struct link_s *link;
   int key;
   int data;
};

void list_add_head(struct node_s *new)
{
   if (head == NULL)
      head = tail = new;
   else {
      new->link.prev = NULL;
      new->link.next = head;
      head = new;
   }
}

void list_add_tail(struct node_s *new)
{
   if (head == NULL)
      head = tail = new;
   else {
      new->link.prev = tail;
      new->link.next = NULL;
      tail = new;
   }
}

void list_del(int key)
{
   struct link_s *p;

   if (head == NULL)
      return NULL;

   for (p = head; p != tail; p = p->link.next) {
      if (((struct node_s *)p)->key == key)
         if (head == tail) { //Single item
            head = tail = NULL;
         }
         else if (p == head) { // At the head
            head = p->next; 
            p->next->prev = NULL; // head->prev = NULL;
         }
         else if (p == tail) { // At the end
            tail = p->prev;
            p->prev->next = NULL; // tail->next = NULL;
         }
         else { // In the middle
         }
      }
   }
}

struct node_s* list_find(int key)
{
   struct link_s *p;

   if (head == NULL)
      return NULL;

   for (p = head; p != tail; p = p->link.next) {
      if (((struct node_s *)p)->key == key)
         return container_of(p, struct node_s, link);
   }
   return NULL;
}

/******************************************************************************/
int main(void)
/******************************************************************************/
{

   return 0;
}

