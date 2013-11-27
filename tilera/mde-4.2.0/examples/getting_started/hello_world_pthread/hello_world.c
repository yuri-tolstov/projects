// This example shows how to create multiple threads (one thread 
// or more threads for every desired tile) and prints "hello world"
// messages in each thread. Use mutex to protect global resources,
// and pthread_cond to synchronize multi pthreads.
//
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <tmc/cpus.h>
#include <tmc/task.h>


//
// Test parameters and their defaults.
//
int n_tiles = 4;

//
// Communication between worker threads and master thread.
//
pthread_mutex_t master_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t master_cond = PTHREAD_COND_INITIALIZER;
int tile_counter = 0;

//
// Function run by each thread, taking a tile number as its argument.
//
void *thread_func(void *arg)
{
  int cpu = (long)arg;

  // Bind to one tile.
  if (tmc_cpus_set_my_cpu(cpu) != 0)
    tmc_task_die("tmc_cpus_set_my_cpu() failed.");

  printf("Thread on tile %d: Hello, Brave New World!\n", cpu);

  pthread_mutex_lock(&master_lock);
  tile_counter++;
  pthread_mutex_unlock(&master_lock);
  pthread_cond_signal(&master_cond);

  return (void*)NULL;
}

//
// main func
//
int main(int argc, char* argv[])
{
  printf("Main process: Hello world!\n");

  //
  // Get desired tiles and make sure we have enough.
  //
  cpu_set_t desired_cpus;
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
    tmc_task_die("tmc_cpus_get_my_affinity() failed.");
  int avail = tmc_cpus_count(&desired_cpus);
  if (avail < n_tiles)
    tmc_task_die("Need %d cpus, but only %d specified in affinity!",
                 n_tiles, avail);

  //
  // Spawn a thread on each tile.
  //
  pthread_t thread;
  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);
  for (int i = 0; i < n_tiles; i++)
  {
    int cpu = tmc_cpus_find_nth_cpu(&desired_cpus, i);
    pthread_create(&thread, &thread_attr, thread_func, (void*)(intptr_t)cpu);
  }

  //
  // Wait for all threads to be created and run.
  //
  pthread_mutex_lock(&master_lock);
  while (tile_counter < n_tiles)
    pthread_cond_wait(&master_cond, &master_lock);
  pthread_mutex_unlock(&master_lock);

  printf("Started %d pthreads\n", tile_counter);

  return 0;
}
