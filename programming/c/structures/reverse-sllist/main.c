
link_t* reverse_list(link_t *head)
{
   link_t *node = head;
   link_t *rhead = NULL;

   while (node != NULL) {
      next = node->next;

         rhead = node;
???      
      node->next = prev;
      
      prev = node;
      node = next;;
   }
   return rhead;
}




