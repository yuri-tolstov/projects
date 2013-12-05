/******************************************************************************/
/* File:   ctrl.c                                                             */
/******************************************************************************/
#include "local.h"

/*Message header*/
typedef struct ccap_mhdr_s {
   int16_t mtype; /*Message type*/
   int16_t dlen; /*Data/payload length*/
   /*Data follows the header.*/
} ccap_mhdr_t;

/******************************************************************************/
void* h2t_thread(void *arg)
/******************************************************************************/
{
   int i, c; /*Index, Return code*/
   gxpci_comp_t comp;
   gxpci_cmd_t cmd;
   chaar *mbuf; /*Message buffer*/
   ccap_mhdr_t *mh; /*Message header*/

   /*Allocate and register data buffers.*/
   tmc_alloc_t alloc = TMC_ALLOC_INIT;
   tmc_alloc_set_huge(&alloc);
   mbuf = tmc_alloc_map(&alloc, MAP_LENGTH);  assert(mbuf);
   mh = mbuf;

   if (gxpci_iomem_register(h2tcon, mbuf, MAP_LENGTH) != 0) {
      tmc_task_die("gxpci_iomem_register(H2T) failed");
   }
   /*Receive and process message from the Host*/
   while (1) {
      /*Post empty buffer.*/
      if (gxpci_get_cmd_credits(h2tcon) <= 0) {
         continue;
      }
      cmd.buffer = mbuf;
      cmd.size = MAX_PKT_SIZE;
      if (gxpci_pq_h2t_cmd(h2tcon, &cmd) < 0) {
         continue;
      }
      /*Get data.*/
      if (gxpci_get_comps(h2tcon, &comp, 0, 1) < 0) {
         continue;
      }
      /*Process the message.*/
      //TODO:
   }
   return NULL;
}

/******************************************************************************/
void* t2h_thread(void *arg)
/******************************************************************************/
{
   int i, c; /*Index, Return code*/
   gxpci_comp_t comp;
   gxpci_cmd_t cmd;
   chaar *mbuf; /*Message buffer*/
   ccap_mhdr_t *mh; /*Message header*/
   int msize; /*Message size*/

   /*Allocate and register data buffers.*/
   tmc_alloc_t alloc = TMC_ALLOC_INIT;
   tmc_alloc_set_huge(&alloc);
   mbuf = tmc_alloc_map(&alloc, MAP_LENGTH);  assert(mbuf);
   mh = mbuf;

   if (gxpci_iomem_register(t2hcon, mbuf, MAP_LENGTH) != 0) {
      tmc_task_die("gxpci_iomem_register(H2T) failed");
   }
   /*Receive Tile internal events and send data to the Host, if necessary.*/
   while (1) {
      //TODO: Receive Internal event
      //
      /*Initialize and post the message.*/
      if (gxpci_get_cmd_credits(t2hcon) <= 0) {
         continue;
      }
      //mh.mtype = xxx;
      //mh.dlen = msize;
      cmd.buffer = mbuf;
      cmd.size = sizeof(ccap_mhdr_t) + msize;
      if (gxpci_pq_t2h_cmd(t2hcon, &cmd) < 0) {
         continue;
      }
      gxpci_get_comps(t2hcon, &comp, 0, 1);
   }
   return NULL;
}

