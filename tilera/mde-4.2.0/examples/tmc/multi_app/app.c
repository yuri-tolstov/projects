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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <tmc/cmem.h>
#include <tmc/sync.h>
#include <tmc/task.h>
#include <tmc/udn.h>


// Parallelize the current process via "fork", or die.
//
// This forks off "tmc_cpus_count(cpus) - 1" (watched) child processes
// and returns the resulting 'rank' in each process (0 for the parent,
// 1, 2, 3, etc. for the children), after locking each process to the
// rank'th cpu in "*cpus".
//
static int
parallelize_onto(cpu_set_t* cpus)
{
  unsigned int count = tmc_cpus_count(cpus);

  int watch_forked_children = tmc_task_watch_forked_children(1);

  int rank;
  for (rank = 1; rank < count; rank++)
  {
    pid_t child = fork();
    if (child < 0)
      tmc_task_die("Failure in 'fork()'.");
    if (child == 0)
      goto done;
  }
  rank = 0;

  (void)tmc_task_watch_forked_children(watch_forked_children);

 done:

  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(cpus, rank)) < 0)
    tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");

  return rank;
}


int
main(void)
{
  // Join (or create).
  if (tmc_cmem_init(0) != 0)
    tmc_task_die("Failure in 'tmc_cmem_init()'.");

  // Join (or create).
  if (tmc_udn_init(NULL) != 0)
    tmc_task_die("Failure in 'tmc_udn_init()'.");

  // Determine the available cpus.
  cpu_set_t cpus;
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
  unsigned int total = tmc_cpus_count(&cpus);

  // Allocate a "barrier" in common memory.
  tmc_sync_barrier_t* barrier =
    (tmc_sync_barrier_t*)tmc_cmem_malloc(sizeof(*barrier));
  if (barrier == NULL)
    tmc_task_die("Failed to allocate barrier.");
  tmc_sync_barrier_init(barrier, total);

  // Create a process for each cpu.
  unsigned int rank = parallelize_onto(&cpus);

  // Activate the hardwall.
  if (tmc_udn_activate() < 0)
    tmc_task_die("Failure in 'tmc_udn_activate()' for rank %d.", rank);

  // At this point, the application is ready to run.  It will share
  // the UDN and common memory with the other application started by
  // start.c.

  // Wait at the barrier.
  tmc_sync_barrier_wait(barrier);

  // Exit.
  return 0;
}
