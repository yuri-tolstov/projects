/******************************************************************************/
/* File:   npu-ptempl/main.c                                                  */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <asm/tilegxpci.h>

/*UI-Proxy Message Types*/
enum {
   UMSG_TYPE_UNDEF,
   UMSG_TYPE_CONFIG,   /*Configure*/
   UMSG_TYPE_START,    /*Start*/
   UMSG_TYPE_STOP,     /*Stop*/
   UMSG_TYPE_RDTRACE,  /*Read trace*/
   UMSG_TYPE_MAX
};

/*TileGx-Proxy Message Types*/
enum {
   TMSG_TYPE_UNDEF,
   TMSG_TYPE_CONFIG,   /*Configure*/
   TMSG_TYPE_START,    /*Start*/
   TMSG_TYPE_STOP,     /*Stop*/
   TMSG_TYPE_RDTRACE,  /*Read trace*/
   TMSG_TYPE_MAX
};

static int qelemsz = RING_BUF_ELEM_SIZE; /*Queue buffer size*/
static int qelems = RING_BUF_ELEMS; /*Queue ring elements.*/


/******************************************************************************/
void* t2u_thread(void *arg)
/******************************************************************************/
/* Tile-to-User Thread.
 * It receives messages from TileGx Card and routes them to the UI Station.
 * TileGx Comm.Channel = Tile packet queue (T2H direction, receive).
 * UI Station Comm.Channel = TCP socket (send).
 */
{
   int i, c, n; /*Index, Code, Number of bytes*/
   host_packet_queue_t q; /*T2H packet queue info*/
   tilepci_packet_queue_info_t qd; /*Queue descriptor.*/
   int fd; /*File descriptor*/

   /*-------------------------------------------------------------------------*/
   /* Open communication channel with TileGx (input).                         */
   /*-------------------------------------------------------------------------*/
   /*Open packet queue.*/
   snprintf(q.dev_name, sizeof(q.dev_name),
            "/dev/tilegxpci0/packet_queue/t2h/%d", cardix, queueix);
   if ((fd = open(q.dev_name, O_RDWR)) < 0) {
      printf("Failed to open T2H channel.\n");
      goto t2u_thread_failed;
   }
   q.pq_regs = (struct gxpci_host_pq_regs_app*)
      mmap(0, sizeof(struct gxpci_host_pq_regs_app),
           PROT_READ | PROT_WRITE,
           MAP_SHARED, fd, TILEPCI_PACKET_QUEUE_INDICES_MMAP_OFFSET);
   assert(q.pq_regs != MAP_FAILED);

   /*Ring buffer.*/
   qd.buf_size = qelemsz;
   if ((c = ioctl(fd, TILEPCI_IOC_SET_PACKET_QUEUE_BUF, &qd)) < 0) {
      printf("Failed to configure ring buffer, errno=%d\n", errno);
      goto t2u_thread_failed;
   }
   /*Get access to the queue data and status info.*/
   n = qelemsz * qelems;
   q.buffer = mmap(0, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
   assert(q.buffer != MAP_FAILED);

   q.pq_status = mmap(0, sizeof(struct tlr_pq_status), PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
   assert(q.pq_status != MAP_FAILED);
   q.pq_regs->consumer_index = 0;

   /*-------------------------------------------------------------------------*/
   /* Open communication channel with UI Station (output).                    */
   /*-------------------------------------------------------------------------*/

   /*-------------------------------------------------------------------------*/
   /* Process messages (from TileGx).                                         */
   /*-------------------------------------------------------------------------*/
   volatile uint32_t *cons = &(q.pq_regs->consumer_index); /*Consumer index*/
   volatile uint32_t *prod = &(q.pq_regs->producer_index); /*Producer index*/
   volatile enum gxpci_chan_status_t *qstat = (q.pq_status->status); /*Queue status*/
   uint32_t read, write; /*Read and Write indexes*/
   //TODO:
   void *umhdr; /*UI message header.*/ 
   void *tmhdr; /*TileGx message header.*/ 

   while (1) {
      /*Get message from TileGx (T2H channel)*/
      if (*status == GXPCI_CHAN_RESET) {
         goto t2u_thread_failed;
      }
      write = *prod;  read = 0;
      while (write != read) {
         read++;
         *cons = read;
         umhdr = q.buffer + read * qelemsz;
         switch (umhdr->mtype) {
         case UMSG_TYPE_CONFIG: break;
         case UMSG_TYPE_START: break;
         case UMSG_TYPE_STOP: break;
         case UMSG_TYPE_RDTRACE: break;
         default: break;
         }
         /*Route message to TileGx*/
         //TODO:...
      }
   } /*while*/

   /*-------------------------------------------------------------------------*/
   /* Exit and error handler.                                                 */
   /*-------------------------------------------------------------------------*/
t2u_thread_failed:
   return NULL;
}

/******************************************************************************/
void* u2t_thread(void *arg)
/******************************************************************************/
/* User-to-Tile Thread.
 * It receives messages from UI Station and routes them to TileGx Card.
 * TileGx Comm.Channel = Tile packet queue (H2T direction, send)
 * UI Station Comm.Channel = TCP socket (receive).
 */
{
   int i, c, n; /*Index, Code, Number of bytes*/
   host_packet_queue_t q; /*H2T packet queue info*/
   tilepci_packet_queue_info_t qd; /*Queue descriptor.*/
   int fd; /*File descriptor*/

   /*-------------------------------------------------------------------------*/
   /* Open communication channel with TileGx (output).                        */
   /*-------------------------------------------------------------------------*/
   snprintf(h2tpq.dev_name, sizeof(h2tpq.dev_name),
            "/dev/tilegxpci0/packet_queue/h2t/%d", cardix, queueix);
   if ((fd = open(q.dev_name, O_RDWR)) < 0) {
      printf("Failed to open H2T channel.\n");
      goto u2t_thread_failed;
   }
   q.pq_regs = (struct gxpci_host_pq_regs_app*)
      mmap(0, sizeof(struct gxpci_host_pq_regs_app),
           PROT_READ | PROT_WRITE,
           MAP_SHARED, fd, TILEPCI_PACKET_QUEUE_INDICES_MMAP_OFFSET);
   assert(q.pq_regs != MAP_FAILED);

   /*Ring buffer.*/
   qd.buf_size = qelemsz;
   if ((c = ioctl(fd, TILEPCI_IOC_SET_PACKET_QUEUE_BUF, &qd)) < 0) {
      printf("Failed to configure ring buffer, errno=%d\n", errno);
      goto u2t_thread_failed;
   }
   /*Get access to the queue data and status info.*/
   n = qelemsz * qelems;
   q.buffer = mmap(0, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
   assert(q.buffer != MAP_FAILED);

   q.pq_status = mmap(0, sizeof(struct tlr_pq_status), PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
   assert(q.pq_status != MAP_FAILED);
   q.pq_regs->producer_index = 0;

   /*-------------------------------------------------------------------------*/
   /* Open communication channel with UI Station (input).                     */
   /*-------------------------------------------------------------------------*/

   /*-------------------------------------------------------------------------*/
   /* Process messages (from UI Station).                                     */
   /*-------------------------------------------------------------------------*/

   /*-------------------------------------------------------------------------*/
   /* Exit and error handler.                                                 */
   /*-------------------------------------------------------------------------*/
u2t_thread_failed:
   return NULL;
}






/*****************************************************************************/
/* Function:   pq_init                                                       */
/*****************************************************************************/
int pq_init(int queueix, host_packet_queue_t *pqinfo)
{
   char devname[64]; /*Device name*/

   
   return 0;
}

/*****************************************************************************/
/* Thread:    H2T                                                            */
/*****************************************************************************/
static void* h2t_thread(void *arg)
{
   const char* path = "/dev/tilegxpci0/h2t/0";
   int fd; /*File descriptors*/
   void *mbuf; /*Output and Input Message buffers*/
   int msize = 4096; /*Message size*/
   int i; /*Index*/

   /*H2T channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", path, strerror(errno));
      return NULL;
   }
   printf("H2T: Opened.\n");
   mbuf = mmap(0, msize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   assert(mbuf != MAP_FAILED);

   /*Construct and initiate Send command*/
   tilepci_xfer_req_t sndcmd = {
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &sndcmd, sizeof(sndcmd)) != sizeof(sndcmd));

      /*Read back the completion status.*/
      tilepci_xfer_comp_t comp;
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("H2T: Written %d\n", i);
      sleep(10);
      i++;
   }
   /*Exit.*/
   printf("H2T: Done.\n");
   close(fd);
   return NULL;
}

/*****************************************************************************/
/* Thread:    T2H                                                            */
/*****************************************************************************/
static void* t2h_thread(void *arg)
{
   const char* path = "/dev/tilegxpci0/t2h/0";
   int fd; /*File descriptors*/
   void *mbuf; /*Output and Input Message buffers*/
   int msize = 4096; /*Message size*/
   int i; /*Index*/

   /*T2H channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", path, strerror(errno));
      return NULL;
   }
   printf("T2H: Opened.\n");
   mbuf = mmap(0, msize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   assert(mbuf != MAP_FAILED);

   /*Construct and initiate Receive command*/
   tilepci_xfer_req_t rxcmd = {
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &rxcmd, sizeof(rxcmd)) != sizeof(rxcmd));

      /*Read back the completion status.*/
      tilepci_xfer_comp_t comp;
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("T2H: Received message %d\n", i);
      i++;
   }
   /*Exit.*/
   printf("T2H: Done.\n");
   close(fd);
   return NULL;
}

/*****************************************************************************/
/* Function:    main                                                         */
/*****************************************************************************/
int main(int argc, char *argv[])
{
   int i; /*Index*/
   pthread_t tid[2]; /*Processes ID*/

   if (pthread_create(&tid[0], NULL, u2t_thread, NULL) != 0) {
      printf("Failed to create H2T thread\n");
      return EIO;
   } 
   if (pthread_create(&tid[1], NULL, t2u_thread, NULL) != 0) {
      printf("Failed to create T2H thread\n");
      return EIO;
   } 
   for (i = 0; i < 2; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

