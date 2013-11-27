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
#include <sys/types.h>

#include <tmc/cpus.h>
#include <tmc/task.h>
#include <tmc/udn.h>


// We will use N cpus to calculate the sum of N * M integers.
#define NUM_CPUS 8
#define INTS_PER_CPU 1024
#define TOTAL_INTS (NUM_CPUS * INTS_PER_CPU)


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


// Compute a partial sum of an array of integers.
//
// Each core first computes the partial sum of a (1 / NUM_CPUS)
// fraction of the array.
//
// Then, given the partial sum on from this core, and the set of cpus
// that we're running on, use the UDN to compute the total sum.  The
// rank 0 cpu sends its sum to rank 1, which accumulates and sends to
// the rank 2 cpu, and so forth.  Finally, the total sum is sent back
// to rank 0, which prints the result.
//
void
sum_of_sums(uint32_t* data, cpu_set_t* cpus, int rank)
{
  // Compute the local sum.
  int start = rank * INTS_PER_CPU;
  int finish = (rank + 1) * INTS_PER_CPU;
  uint32_t local_sum = 0;
  for (int i = start; i < finish; i++)
    local_sum += data[i];

  // Figure out which tiles to communicate with over the UDN, and
  // compute the sum-of-sums.
  int next_rank = rank + 1;
  if (next_rank == NUM_CPUS)
    next_rank = 0;

  int next_cpu = tmc_cpus_find_nth_cpu(cpus, next_rank);
  DynamicHeader header = tmc_udn_header_from_cpu(next_cpu);
  
  if (rank == 0)
  {
    tmc_udn_send_1(header, UDN0_DEMUX_TAG, local_sum);
    uint32_t total_sum = tmc_udn0_receive();
    printf("Sum of 0 to %d = %d\n", (TOTAL_INTS - 1), total_sum);
  }
  else
  {
    uint32_t accum = tmc_udn0_receive();
    accum += local_sum;
    tmc_udn_send_1(header, UDN0_DEMUX_TAG, accum);
  }
}


int
main(int argc, char**argv)
{
  // Make sure we have enough cpus.
  cpu_set_t cpus;
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");

  // Allocate an array of integers.  We use tmc_alloc_set_shared() to allocate
  // memory that will be shared in all child processes.  This mechanism is
  // sufficient if an application can allocate all of its shared memory
  // in the parent.  If an application needs to dynamically allocate
  // shared memory in child processes, use the tmc_cmem APIs.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_shared(&alloc);
  uint32_t* data = tmc_alloc_map(&alloc, sizeof(uint32_t) * TOTAL_INTS);
  if (data == NULL)
    tmc_task_die("Failed to allocate memory.");
  for (uint32_t i = 0; i < TOTAL_INTS; i++)
    data[i] = i;

  // Reserve the UDN rectangle that surrounds our cpus.
  if (tmc_udn_init(&cpus) < 0)
    tmc_task_die("Failure in 'tmc_udn_init(0)'.");

  // Go parallel.
  int rank = parallelize(NUM_CPUS);

  // Now that we're bound to a core, attach to our UDN rectangle.
  if (tmc_udn_activate() < 0)
    tmc_task_die("Failure in 'tmc_udn_activate()'.");

  // Compute and print the sum-of-sums using the UDN.
  sum_of_sums(data, &cpus, rank);
}
