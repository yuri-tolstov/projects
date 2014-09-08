#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");

static struct workqueue_struct *mywq;

typedef struct {
   struct work_struct mywork;
   int x;
} mywork_t;

#if 0
================================================================================
--------------------------------------------------------------------------------
struct workqueue_struct {
--------------------------------------------------------------------------------
       struct list_head        pwqs;           /* WR: all pwqs of this wq */
       struct list_head        list;           /* PL: list of all workqueues */
       struct mutex            mutex;          /* protects this wq */
       int                     work_color;     /* WQ: current work color */
       int                     flush_color;    /* WQ: current flush color */
       atomic_t                nr_pwqs_to_flush; /* flush in progress */
       struct wq_flusher       *first_flusher; /* WQ: first flusher */
       struct list_head        flusher_queue;  /* WQ: flush waiters */
       struct list_head        flusher_overflow; /* WQ: flush overflow list */
       struct list_head        maydays;        /* MD: pwqs requesting rescue */
       struct worker           *rescuer;       /* I: rescue worker */
       int                     nr_drainers;    /* WQ: drain in progress */
       int                     saved_max_active; /* WQ: saved pwq max_active */
       struct workqueue_attrs  *unbound_attrs; /* WQ: only for unbound wqs */
       struct pool_workqueue   *dfl_pwq;       /* WQ: only for unbound wqs */
#ifdef CONFIG_SYSFS
       struct wq_device        *wq_dev;        /* I: for sysfs interface */
#endif
#ifdef CONFIG_LOCKDEP
       struct lockdep_map      lockdep_map;
#endif
       char                    name[WQ_NAME_LEN]; /* I: workqueue name */
       /* hot fields used during command issue, aligned to cacheline */
       unsigned int            flags ____cacheline_aligned; /* WQ: WQ_* flags */
       struct pool_workqueue __percpu *cpu_pwqs; /* I: per-cpu pwqs */
       struct pool_workqueue __rcu *numa_pwq_tbl[]; /* FR: unbound pwqs indexed by node */
};
--------------------------------------------------------------------------------
struct work_struct {
--------------------------------------------------------------------------------
       atomic_long_t data;
       struct list_head entry;
       work_func_t func;
#ifdef CONFIG_LOCKDEP
       struct lockdep_map lockdep_map;
#endif
};

typedef void (*work_func_t)(struct work_struct *work);
================================================================================
#endif


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
/*
 * boot queue_work(struct workqueue_struct *wq, struct work_struct *work)
 * {
 *    int cpu = smp_processor_id();
 *    ulong_t flags;
 *    local_irq_save(flags);
 *    if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
 *       __queue_work(cpu, wq, work);  //This is big function... in workqueue.c
 *    }
 *    local_irq_restore(flags);
 * }
 */

void cleanup_module(void)
{
   flush_workqueue(mywq);
   destroy_workqueue(mywq);
}

