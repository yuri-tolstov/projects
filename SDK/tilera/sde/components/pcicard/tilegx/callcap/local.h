/******************************************************************************/
/* File:   local.h                                                            */
/******************************************************************************/
#ifndef __LOCAL_H__
#define __LOCAL_H__
/*----------------------------------------------------------------------------*/
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/dataplane.h>

#include <asm/tilegxpci.h>
#include <arch/atomic.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>
#include <gxio/trio.h>
#include <gxpci/gxpci.h>

#include <tmc/mem.h>
#include <tmc/alloc.h>
#include <tmc/task.h>
#include <tmc/sync.h>
#include <tmc/spin.h>
#include <tmc/cpus.h>
/*----------------------------------------------------------------------------*/

/*Constants.*/
#define NUMLINKS   4  /*Number of network interfaces*/
#define DTILEBASE  4  /*Dataplane TILE base*/

/*General global data and functions.*/
extern tmc_sync_barrier_t syncbar;
extern tmc_spin_barrier_t spinbar;

void* h2t_thread(void *arg);
void* t2h_thread(void *arg);
void* net_thread(void *arg);

/*Host-Tile communication channel.*/
extern gxpci_context_t *h2tcon;
extern gxpci_context_t *t2hcon;

/*Call processing*/
int ccap_detect_call(gxio_mpipe_idesc_t *idesc);

/*Trace buffer processing*/
void ccap_trace_init(int nrecs, int mreclen);
void ccap_trace_add(int iifx, gxio_mpipe_idesc_t *idesc);

/*----------------------------------------------------------------------------*/
#endif /*__LOCAL_H__*/
