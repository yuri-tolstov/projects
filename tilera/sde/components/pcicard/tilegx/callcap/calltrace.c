/******************************************************************************/
/* File: callcap/calltrace.c                                                  */
/******************************************************************************/
#include "local.h"

/******************************************************************************/
/* Call Detection.                                                            */
/******************************************************************************/
/*============================================================================*/
int ccap_detect_call(gxio_mpipe_idesc_t *idesc)
/*============================================================================*/
{
   return 0;
}

/******************************************************************************/
/* Trace Buffer.                                                              */
/******************************************************************************/
//NOTES:
// 1. Each Tile, processing specific Ingress port, work with its own Trace Buffer.
//    No locks are necessary.
// 2. Tracing stops, as soon as the 1st trace buffer is full.
//    Therefore, some variables are shared: trcen, recpktl, mode;
//=============================================================================
#define CCAP_PKTBUF_SIZE  0x100000 /*1MB*/
#define CCAP_NUMLINKS  4 /*Number of Links*/

/*Global Trace Control.*/
static int trcen; /*Start/Stop switch: 0=disable, 1=enable*/
static int mode; /*Method of recording: 0=till full, 1=circular buffer*/
static int avepktl; /*Average recorded packet length.*/
static int numrecs; /*Number of records per packet buffer.*/

/*Trace Record*/
typedef struct record_descr_s {
   uint64_t tm; /*Timestamp, in CPU cycles*/
   uint16_t pktlen; /*Packet size*/
   uint16_t reclen; /*Recorded packet size*/
   uint32_t __nu; /*Alignment to 64-bit boundary*/
   char *pktb; /*Packet buffer (one packet)*/
   /*pktb is calculated as aligned(pktb + reclen) in the previous slot.*/
} record_descr_t;

/*Trace Control Block*/
typedef struct trace_ctrl_s {
   int recix; /*Index of last current record*/
   int nrecs; /*Number of records in the buffer*/
   record_descr_t *rcb; /*Block of record descriptors*/
   char *pktbuf; /*Packet buffer*/
} trace_ctrl_t;
static trace_ctrl_t trccblk[CCAP_NUMLINKS]; /*Trace Control Blocks*/


/*============================================================================*/
void ccap_trace_init(int nrecs, int apktl)
/*============================================================================*/
/* 1. Allocate 4 packet buffers -- one per link.
 * 2. Split Packet Buffer on slots each having size of Max. recorded length.
 * 3. Initialize Global Trace Control structure.
 */
{
   trace_ctrl_t *tcb; /*Trace Control Block*/

   numrecs = nrecs;  avepktl = apktl;
   trcen = 0;
   mode = 0;

   for (i = 0; i < CCAP_NUMLINKS; i++) {
      tcb = &trccblk[i];
      tcb->recix = 0;  tcb->nrecs = 0;
      tcb->rcb = calloc(numrecs, sizeof(record_descr_t));  assert(tcb->rcb != NULL);
      tcb->pktbuf = malloc(numrecs * avepktl);  assert(tcb->pktbuf != NULL);
      if (i = 0) tcb->rcb[0].pktb = tcb->pktbuf;
   }
}

/*============================================================================*/
void ccap_trace_add(int iifx, gxio_mpipe_idesc_t *idesc)
/*============================================================================*/
{
   trace_trccb_t *tcb; /*Trace Control Block*/
   record_descr_t *rcbc, *rcbn; /*Current and Next record control block/descriptors*/

   if (trcen) {
      tcb = trccblk[iifx];
      if (tcb->recix < (tcb->numrecs - 1)) {
         rcbc = &tcb->rcb[tcb->recix];
         rcbc->tm = xxx();
         memcpy(rcbc->pktb, idesc->xxx, idesc->yyy);
         tcb->recix++;
         rcbn = &tcb->rcb[tcb->recix];
         rcbn->pktb = rcbc->pktb + rcbc->reclen + __alignment__;
      }
   }
}

