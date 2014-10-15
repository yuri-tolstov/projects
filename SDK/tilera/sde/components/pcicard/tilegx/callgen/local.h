/******************************************************************************/
/* File:   local.h                                                            */
/******************************************************************************/
#ifndef __LOCAL_H__
#define __LOCAL_H__
/*----------------------------------------------------------------------------*/

/*Constants.*/
#define NUMLINKS   4  /*Number of network interfaces*/
#define DTILEBASE  4  /*Dataplane TILE base*/

/*Data.*/
extern tmc_sync_barrier_t syncbar;
extern tmc_spin_barrier_t spinbar;

/*Functions*/
void* h2t_thread(void *arg);
void* t2h_thread(void *arg);
void* net_thread(void *arg);

/*----------------------------------------------------------------------------*/
#endif /*__LOCAL_H__*/
