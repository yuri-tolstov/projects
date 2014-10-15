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
#include <unistd.h>

#include <tmc/cmem.h>
#include <tmc/mem.h>
#include <tmc/sync.h>


// Fork "total - 1" children, and return the "rank" of each process,
// with the parent itself having rank "0".
//
static unsigned int
fork_processes(unsigned int total)
{
  for (unsigned int i = 1; i < total; i++)
  {
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0)
    {
      tmc_cmem_init(0);
      return i;
    }
  }

  return 0;
}


typedef struct _info_t
{
  // A mutex.
  tmc_sync_mutex_t mutex;

  // A barrier.
  tmc_sync_barrier_t barrier;

  // A string (allocated in common memory).
  const char* str;

} info_t;


int
main(void)
{
  tmc_cmem_init(0);

  // Allocate a full page of zeroed common memory.
  info_t* info = (info_t*)tmc_cmem_calloc(1, sizeof(*info));
  assert(info != NULL);

  // Create a total of 4 processes.
  unsigned int total = 4;

  // Initialize the mutex and barrier.
  tmc_sync_mutex_init(&info->mutex);
  tmc_sync_barrier_init(&info->barrier, total);

  // Create the other processes.
  unsigned int rank = fork_processes(total);

  // Let one process allocate a common string.
  if (rank == 2)
  {
    tmc_alloc_t alloc = TMC_CMEM_ALLOC_INIT;
    char* str = tmc_alloc_map(&alloc, getpagesize());

    snprintf(str, getpagesize(), "Rank %d has pid %d", rank, getpid());

    // Use a memory fence to make sure the other processes can see the
    // string, and then let them know about the string.
    tmc_mem_fence();
    info->str = str;
  }

  // Everyone now waits on the barrier.
  tmc_sync_barrier_wait(&info->barrier);

  // Output the string, using the mutex to avoid interleaving output.
  tmc_sync_mutex_lock(&info->mutex);
  printf("Rank %d has pid %d and sees: %s\n", rank, getpid(), info->str);
  tmc_sync_mutex_unlock(&info->mutex);

  // Wait on the barrier again so everyone's printf() gets out.
  tmc_sync_barrier_wait(&info->barrier);

  // Exit.
  return 0;
}
