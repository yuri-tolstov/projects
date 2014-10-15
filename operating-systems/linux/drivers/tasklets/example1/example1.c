#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");

char mydata[] = "My Tasklet function was called";

/*Bottom-Half function*/
void myfunc(unsigned long data)
{
   printk("%s\n", (char *)data);
}

DECLARE_TASKLET(mytask, myfunc, (unsigned long)&mydata);
/*
 * DECLARE_TASKLET =
 *    tasklet_struct mytask = {NULL, 0, ATOMIC_INIT(0), myfunc, mydata)
 *
 * struct tasklet_struct {
 *    struct tasklet_struct *next;
 *    ulong_t state;
 *    atomic_t count;
 *    void (*func)(ulong_t);
 *    ulong_t data;
 * };
 */

int init_module(void)
{
   /*Schedule the Bottom-Half*/
   tasklet_schedule(&mytask);
   return 0;
}
/*
 * tasklet_schedule(struct tasklet_struct *t)
 * {
 *    int cpu = smp_processor_id();
 *    ulong_t flags;
 *
 *    local_irq_safe(flags);
 *    t->next = tasklet_vec[cpu].list;
 *    tasklet_vec[cpu].list = t;
 *    __cpu_raise_softirq(cpu, TASKLET_SOFTIRQ);
 *    local_irq_restore(flags);
 * }
 *
 * struct tasklet_head tasklet_vec[NR_CPUS];
 */

void cleanup_module(void)
{
   tasklet_kill(&mytask);
}

