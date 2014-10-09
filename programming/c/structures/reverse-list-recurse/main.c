void recursiveReverse(struct node** head_ref)
02	{
03	    struct node* first;
04	    struct node* rest;
05	 
06	    /* empty list */
07	    if (*head_ref == NULL)
08	       return;
09	 
10	    first = *head_ref;
11	    rest  = first->next;
12	 
13	    /* List has only one node */
14	    if (rest == NULL)
15	       return;
16	 
17	    /* put the first element on the end of the list */
18	    recursiveReverse(&rest);
19	    first->next->next  = first;
20	 
21	    /* tricky step */
22	    first->next  = NULL;
23	 
24	    /* fix the head pointer */
25	    *head_ref = rest;
26	}

http://www.crazyforcode.com/linked-list/


