#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");

static struct workqueue_struct *mywq;

typedef struct {
   struct work_struct mywork;
   int x;
} mywork_t;

mywork_t *work, *work2;

/*Bottom-Half Handler*/
static void mywq_function(struct work_struct *work)
{
   mywork_t *w = (mywork_t *)work;
   printk("mywork.x = %d\n", w->x);
   kfree((void *)work);
}

/*Workqueue API functions*/
int init_module(void)
{
   if ((mywq = create_workqueue("myque")) > 0) {
      /*Queue Work 1*/
      if ((work = kmalloc(sizeof(mywork_t), GFP_KERNEL)) > 0) {
         INIT_WORK((struct work_struct *)work, mywq_function);
         work->x = 1;
         queue_work(mywq, (struct work_struct *)work);
      }
      /*Queue Work 2*/
      if ((work2 = kmalloc(sizeof(mywork_t), GFP_KERNEL)) > 0) {
         INIT_WORK((struct work_struct *)work2, mywq_function);
         work2->x = 2;
         queue_work(mywq, (struct work_struct *)work2);
      }
   }
   return 0;
}

void cleanup_module(void)
{
   flush_workqueue(mywq);
   destroy_workqueue(mywq);
}

