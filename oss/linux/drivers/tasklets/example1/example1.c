#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");

char mydata[] = "My Tasklet function was called";

/*Bottom-Half function*/
void mytask_function(unsigned long data)
{
   printk("%s\n", (char *)data);
}

DECLARE_TASKLET(mytask, mytask_function, (unsigned long)&mydata);

int init_module(void)
{
   /*Schedule the Bottom-Half*/
   tasklet_schedule(&mytask);
   return 0;
}

void cleanup_module(void)
{
   tasklet_kill(&mytask);
}

