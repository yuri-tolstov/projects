
static volatile int mutex = 0;

static inline int memread(volatile int *addr) {
   return *((volatile int *)addr);
}

static inline void memwrite(volatile int *addr, int data) {
   *((volatile int *)addr) = data;
}

static inline void mutex_lock(void) {
   while (memread(&mutex) != 0) {
      udelay(2);
   }
   memwrite(&mutex, 1);
}

static inline void mutex_unlock(void) {
   memwrite(&mutex, 0);
}

