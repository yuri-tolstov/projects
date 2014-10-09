#include <stdio.h>
#include <stdlib.h>
/****************************************************************************** 
http://kernelnewbies.org/FAQ/LinkedLists

Linux List is defined in:
   include/linux/list.h

The specifics:

1. The Linux list -- a curcular linked list.
2. Every item contains a (circular) list (endo-list).
   Vs. as normally, items are lined up in a list (exo-list).
3. New node is added after the head by function:
   list_add(struct list_head *new, struct list_head *head);
      head => new
*******************************************************************************/
/* Linux implementation of link lists */
struct list_head {
   struct list_head *next;
   struct list_head *prev;
};

void list_add(struct list_head *new, struct list_head *head) {
   struct list_head *prev = head;
   struct list_head *next = head->next;

   next->prev = new;
   new->next = next;
   new->prev = prev;
   prev->next = new; 
}

#define container_of(__ptr, __type, __member) ({ \
   const typeof(((__type *)0)->__member) *__mp = (__ptr); \
   (__type *)((char *)__mp - offsetof(__type, __member)); \
})

#define list_entry(__ptr, __type, __member) \
   containter_of(__ptr, __type, __member)

#define list_for_each(__pos, __head) \
   for (__pos = (__head)->next; __pos != (__head); __pos = __pos->next)

#define INIT_LIST_HEAD(name) {&(name), &(name)}

/* My stuff */
typedef struct mynode_s {
   int data;
   struct list_head list;
} mynode_t;

void add_node(struct list_head *head, int data)
{
   mynode_t *node = malloc(sizeof(mynode_t));
   node->data = data;
   INIT_LIST_HEAD(node->list);
   list_add(&node->list, head);
}

int del_node(struct list_head *head, int key)
{
   struct list_node *it;
   mynode_t *node;

   list_for_each(it, head) {
      node = list_entry(it, mynode_t, list);
      if (node->data == key) {
         list_del(&node->list);
         free(node);
         return 1;
      } 
   }
   return 0;
}

void show_list(struct list_head *head)
{
   struct list_head *it;
   mynode_t *node;
   list_for_each(it, head) {
      node = list_entry(it, mynode_t, list);
      printf("%d\n", node->data);
   }
}

/******************************************************************************/
int main(void)
/******************************************************************************/
{
   LIST_HEAD(listhead);
   add_node(&listhead, 1);
   add_node(&listhead, 2);
   add_node(&listhead, 3);
   show_list(&listhead);

   del_node(&listhead, 3);
   show_list(&listhead);
   del_node(&listhead, 1);
   del_node(&listhead, 2);
   show_list(&listhead);

   return 0;
}

