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

#include <tmc/alloc.h>

#include <tmc/cpus.h>
#include <tmc/sync.h>
#include <tmc/task.h>


// Parallelize the current process via "fork", or die.
//
// This forks off "count - 1" (watched) child processes and returns the
// resulting 'rank' in each process (0 for the parent, 1, 2, 3, etc. for
// the children), after locking each process to the rank'th cpu in the
// current affinity set.
//
static int
parallelize(int count)
{
  cpu_set_t cpus;

  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");

  if (tmc_cpus_count(&cpus) < count)
    tmc_task_die("Insufficient cpus (%d < %d).", tmc_cpus_count(&cpus), count);

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

  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank)) < 0)
    tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");

  return rank;
}


int main(void)
{
  // Total number of processes.
  unsigned int total = 4;

  // Allocate a "barrier" in memory which will be shared between the
  // parent process and its children.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_shared(&alloc);
  tmc_sync_barrier_t* barrier =
    (tmc_sync_barrier_t*)tmc_alloc_map(&alloc, sizeof(*barrier));
  if (barrier == NULL)
    tmc_task_die("Failed to allocate memory for barrier.");
  tmc_sync_barrier_init(barrier, total);

  // Create some processes.
  int rank = parallelize(total);

  // Ask the shepherd to monitor our stdout/stderr.  This will
  // have no effect unless "tmc_task_has_monitor()" is true.
  tmc_task_monitor_console();

  // Emit some output.
  printf("Process %d: Hello World!\n", rank);

  // Wait for all output to complete.
  tmc_sync_barrier_wait(barrier);

  // Termination requires a shepherd.
  if (tmc_task_has_shepherd())
  {
    // Let one of the processes terminate the entire app.
    // The actual choice of process here is arbitrary.
    if (rank == 2)
    {
      // Terminate the entire app.
      tmc_task_terminate_app();

      // Termination should affect this process too.
      tmc_task_die("Failed to terminate app.");
    }

    // Wait to be terminated.
    while (1)
      pause();
  }

  return 0;
}
