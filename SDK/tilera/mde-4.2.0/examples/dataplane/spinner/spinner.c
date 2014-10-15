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

// This example demonstrates how to bind threads to dataplane CPUs and
// verify that the linux kernel has, in fact, stopped taking timer
// ticks.  Each 'spinner' thread runs a loop for a specified period of
// time, verifying that each iteration never takes longer than we
// would expect for the occassional random cache miss.

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/dataplane.h>
#include <tmc/cpus.h>
#include <tmc/task.h>
#include <tmc/perf.h>
#include <tmc/spin.h>
#include <arch/cycle.h>


// How many seconds to spin.
int spin_secs;

// The maximum number of cycles the spinner will tolerate before
// failing on the assumption some Linux interrupt occurred.
enum { MAX_ITERATION_CYCLES = 2000 };


// CPU speed in hertz.
uint64_t hertz;

tmc_spin_barrier_t barr;


// Spin for spin_secs seconds and make sure we don't have any
// unexpected latencies.
void do_spin(int cpu)
{
  uint64_t base, end, last, now;

  base = get_cycle_count();
  end = base + hertz * spin_secs;
  last = base;
  while ((now = get_cycle_count()) < end)
  {
    if (now > last + MAX_ITERATION_CYCLES)
    {
      set_dataplane(0);   // disable DP_STRICT so we can call printf()
      printf("cpu %d: %lld cycles interrupt\n",
             cpu, (long long)(now - last));
      return;
    }
    last = now;
  }
}


// The work function run by each thread.
void* start_thread(void* arg)
{
  int cpu = (intptr_t) arg;

  if (tmc_cpus_set_my_cpu(cpu) != 0)
    tmc_task_die("tmc_cpus_set_my_cpu() failed.");

  mlockall(MCL_CURRENT);
  tmc_spin_barrier_wait(&barr);
  if (set_dataplane(DP_STRICT | DP_DEBUG) != 0)
    tmc_task_die("set_dataplane() failed.");

  // Spin and confirm we are in ZOL mode.
  do_spin(cpu);

  set_dataplane(0);
  tmc_spin_barrier_wait(&barr);

  return NULL;
}


int main(int argc, char **argv)
{
  if (argc != 2)
  {
 syntax:
    fprintf(stderr, "syntax: spinner SECONDS\n");
    exit(1);
  }
    
  char *endp;
  spin_secs = strtoul(argv[1], &endp, 10);
  if (endp == argv[1] || *endp != '\0')
    goto syntax;

  hertz = tmc_perf_get_cpu_speed();

  // Get set of dataplane tiles.
  cpu_set_t dataplane;
  if (tmc_cpus_get_dataplane_cpus(&dataplane) != 0)
    tmc_task_die("tmc_cpus_get_dataplane_cpus() failed.");

  // Determine number of dataplane tiles.
  int n = tmc_cpus_count(&dataplane);

  // Require at least 1 dataplane tile.
  if (n < 1)
    tmc_task_die("There are no dataplane tiles.");

  // Initialize global data.
  tmc_spin_barrier_init(&barr, n);

  // Create threads.
  pthread_t handles[TMC_CPUS_MAX_COUNT] = { 0 };
  for (int i = 0; i < n; ++i)
  {
    int cpu = tmc_cpus_find_nth_cpu(&dataplane, i);
    if (pthread_create(&handles[i], NULL, start_thread,
                       (void*)(intptr_t)cpu) != 0)
      tmc_task_die("pthread_create() failed: %s", strerror(errno));
  }

  // Wait for all the other threads to exit.
  for (int i = 0; i < n; ++i)
  {
    if (pthread_join(handles[i], NULL) != 0)
      tmc_task_die("pthread_join() failed: %s", strerror(errno));
  }

  printf("Test finished.\n");
  
  return 0;
}
