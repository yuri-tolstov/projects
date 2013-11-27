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

// Simple fork/exec-based application template.

#include <stdio.h>  // printf()
#include <unistd.h> // fork(), getpid()
#include "parallel_utils.h" // tmc-based utilities

/** Main function. */
int main(int argc, char** argv)
{
  // Number of instances of this program to run
  // (including the initial parent process).
  int instances = 4;

  // Detect whether we're the parent or an exec'd child,
  int is_parent = is_parent_process();

  // Get the application's affinity set.
  // We'll use the first N available cpus from this set.
  // NOTE: this means parent should _not_ call any functions
  // that shrink the affinity set prior to go_parellel();
  cpu_set_t cpus;
  int status = tmc_cpus_get_my_affinity(&cpus);
  check_tmc_status(status, "tmc_cpus_get_my_affinity()");

  // Define UDN cpu set as first N available cpus
  status = udn_init(instances, &cpus);
  check_tmc_status(status, "udn_init()");

  // Initialize "common" shared memory with default size.
  status = tmc_cmem_init(0);
  check_tmc_status(status, "tmc_cmem_init()");

  // Allocate barrier data structure in shared memory.
  tmc_sync_barrier_t* barrier = NULL;
  if (is_parent)
  {
    // Allocate/initialize barrier data structure in common memory.
    barrier = (tmc_sync_barrier_t*) tmc_cmem_malloc(sizeof(*barrier));
    if (barrier == NULL)
      tmc_task_die("barrier_init(): "
        "Failed to allocate barrier data structure.");
    tmc_sync_barrier_init(barrier, instances);
  }

  // Pass the barrier pointer to any exec'd children.
  share_pointer("SHARED_BARRIER_POINTER", (void**) &barrier);

  // Fork/exec any additional child processes,
  // each locked to its own tile,
  // and get index [0 -- instances-1] of current process.
  int index = go_parallel(instances, &cpus, argc, argv);
  pid_t pid = getpid();
  printf("Process(pid=%i), index=%i: started.\n", pid, index);

  // Enable UDN access for this process (parent or child).
  // Note: this needs to be done after we're locked to a tile.
  status = tmc_udn_activate();
  check_tmc_status(status, "tmc_udn_activate()");

  // Wait here until all other processes have caught up.
  tmc_sync_barrier_wait(barrier);

  // Send/receive a value over the UDN.
  int from = 0;
  int to = instances - 1;
  if (index == from)
  {
    int value = 42;
    printf("Process(pid=%i), index=%i: sending value %i to cpu %i...\n",
        pid, index, value, to);
    udn_send_to_nth_cpu(to, &cpus, value);
    printf("Process(pid=%i), index=%i: sent value %i to cpu %i.\n",
        pid, index, value, to);
  }
  else if (index == to)
  {
    int received = 0;
    printf("Process(pid=%i), index=%i: receiving value...\n",
        pid, index);
    received = udn_receive();
    printf("Process(pid=%i), index=%i: received value %i...\n",
        pid, index, received);
  }

  // Wait here until all other processes have caught up.
  tmc_sync_barrier_wait(barrier);

  printf("Process(pid=%i), index=%i: finished.\n",
      pid, index);

  // We're done.
  return 0;
}
