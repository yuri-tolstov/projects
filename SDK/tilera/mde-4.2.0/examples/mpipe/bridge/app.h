// Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//   The source code contained or described herein and all documents
//   related to the source code ("Material") are owned by Tilera
//   Corporation or its suppliers or licensors.  Title to the Material
//   remains with Tilera Corporation or its suppliers and licensors. The
//   software is licensed under the Tilera MDE License.
//
//   Unless otherwise agreed by Tilera in writing, you may not remove or
//   alter this notice or any other notice embedded in Materials by Tilera
//   or Tilera's suppliers or licensors in any way.
//
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/dataplane.h>

#include <tmc/alloc.h>

#include <arch/atomic.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/spin.h>
#include <tmc/sync.h>
#include <tmc/task.h>

// This is a special queue number used by the worker to drop packets.
#define DROP_QUEUE 0


// This function is called exactly once before workers are started up.
extern void app_init();

// This is called if the run ends (e.g. if number of packets given).
extern void app_stats();

// Main packet handling routine.  This function's job is to return the output
// port(s) for the given idesc.  
extern uint32_t app_handle_packet(gxio_mpipe_idesc_t *idesc, 
                                  int input_port);

// This function called once every ~2 seconds.  
extern void app_maintanance_operations();
