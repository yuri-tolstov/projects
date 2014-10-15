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

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>					
#include <assert.h>

#include <arch/cycle.h>     

#include <tmc/task.h>       
#include <tmc/cpus.h>
#include <tmc/spin.h>

#ifdef USE_BASIC_QUEUE
#include <tmc/queue.h>
#elif defined(USE_CACHELINE_QUEUE)
#include <tmc/cacheline_queue.h>
#else
#error "Must define a queue type!"
#endif // USE_BASIC_QUEUE

#define LG2_CAPACITY           (10)
#define RUN_MULTIPLIER_DEFAULT (50)
#define MANY                   (1)
#define NUM_THREADS            (1 + MANY)
#define STRIDE                 (4)
#define unlikely(cond)          __builtin_expect((cond), 0)

TMC_QUEUE(my_queue, uint64_t, LG2_CAPACITY, \
          (TMC_QUEUE_SINGLE_SENDER | TMC_QUEUE_SINGLE_RECEIVER));

uint32_t           g_run_multiplier = (uint32_t)RUN_MULTIPLIER_DEFAULT;
pthread_t          threads[NUM_THREADS];				
cpu_set_t          cpus;												
my_queue_t*        queue;
tmc_spin_barrier_t barrier = TMC_SPIN_BARRIER_INIT(NUM_THREADS);


// Match argument list option.
// Options ending in '=' take an additional value, which may be
// attached in the same argument or detached in the following
// argument.
// @param arglist	Points to remaining argv, updated on match.
// @param option	The option to match, ending in '=' if it takes a value.
// @return		Value if option matches, NULL otherwise.
//
static char
*shift_option(char ***arglist, const char *option)
{
  char **args = *arglist;
  char *first = args[0], **rest = &args[1];

  int optlen = strlen(option);
  char *val = first + optlen;

  if (option[optlen-1] != '=')
  {

    // Simple option without operand.
    //
    if (strcmp(first, option))
      return NULL;
  }
  else
  {
    // Option with operand.
    //
    if (strncmp(first, option, optlen - 1))
      return NULL;

    // Look for operand attached or in next argument.
    //
    if (first[optlen - 1] == '\0')
      val = *rest++;
    else if (first[optlen - 1] != '=')
      return NULL;
  }

  // Update argument list.
  //
  *arglist = rest;
  return val;
}


uint64_t bench_sender(int count)
{
  tmc_spin_barrier_wait(&barrier);

  uint64_t start = get_cycle_count();
  
  for (uint64_t i = 0; i < count; i++)
  {
    while(unlikely(my_queue_enqueue(queue, i) != 0))
    {}
  }

  uint64_t finish = get_cycle_count();
  
  tmc_spin_barrier_wait(&barrier);

  return finish - start;
}


uint64_t bench_receiver(int count)
{
  uint64_t data;

  tmc_spin_barrier_wait(&barrier);

  uint64_t start = get_cycle_count();
  
  for (uint64_t i = 0; i < count; i++)
  {
    while(unlikely(my_queue_dequeue(queue, &data) != 0))
    {}
  }

  uint64_t finish = get_cycle_count();
  
  tmc_spin_barrier_wait(&barrier);

  return finish - start;
}

uint64_t bench_sender_multiple(int count)
{
  tmc_spin_barrier_wait(&barrier);

  uint64_t data_array[STRIDE];
  
  for (uint64_t i = 0; i < STRIDE; i++)
  {
    data_array[i] = i;
  }

  uint64_t start = get_cycle_count();

  for (uint64_t i = 0; i < (count / STRIDE); i++)
  {
    while (unlikely(my_queue_enqueue_multiple(queue, data_array, STRIDE) != 0))
    {}
  }

  uint64_t finish = get_cycle_count();

  tmc_spin_barrier_wait(&barrier);

  return finish - start;
}


// The worker function for each thread.  The sender injects many items
// into the queue, and the receiver consumes them.
// 
void* thread_func(void* arg)
{
  int rank = (intptr_t) arg;									
  int capacity = 1 << LG2_CAPACITY;				
  int count = capacity * g_run_multiplier;	
  
  // Bind this thread to the rank'th core in the cpu set.
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank)) < 0)
    tmc_task_die("tmc_cpus_set_my_cpu() failed.");
  
  if (rank == 1)
  {
    queue = memalign(64, sizeof(*queue));
    assert(queue != NULL);
    my_queue_init(queue);
  }
  
  tmc_spin_barrier_wait(&barrier);

  // Single obj enqueue.
  uint64_t cycles;
 
  if (rank == 0)
    cycles = bench_sender(count);
  else
    cycles = bench_receiver(count);

  uint64_t reverse_cycles;
  
  if (rank == 1)
    reverse_cycles = bench_sender(count);
  else
    reverse_cycles = bench_receiver(count);

  if (rank == 0)
  {
    printf("One-to-one cycles per transfer: %0.2f\n",
           (float) cycles / count);
    printf("One-to-one cycles per transfer (reverse): %0.2f\n",
           (float) reverse_cycles / count);
  }

  // Multiple obj enqueue.
  if (rank == 0)
    cycles = bench_sender_multiple(count);
  else
    cycles = bench_receiver(count);
  
  if (rank == 1)
    reverse_cycles = bench_sender_multiple(count);
  else
    reverse_cycles = bench_receiver(count);

  if (rank == 0)
  {
    printf("Multiple one-to-one cycles per transfer: %0.2f\n",
           (float) cycles / count);
    printf("Multiple one-to-one cycles per transfer (reverse): %0.2f\n",
           (float) reverse_cycles / count);
  }
  
  return NULL;
}
     

// This method creates (count - 1) child threads and calls 'func' in
// each thread, passing the rank to each thread.  The caller is
// assigned the lowest rank.
//
void
run_threads(int count, void*(*func)(void*))
{
  for (int i = 1; i < count; i++)
  {
    if (pthread_create(&threads[i], NULL, func, (void*)(intptr_t)i) != 0)
      tmc_task_die("pthread_create() failed.");
  }
  
  (void) func((void*) 0);  
}


// Wait for all threads to finish.
//
void 
finish_threads(int count)
{
  for (int i = 1; i < count; i++) 
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("pthread_join() failed.");
  }
}


int 
main(int argc, char**argv)
{	
  // Scan options.
  char **args = &argv[1];
  while (*args)
  {
    char *opt = NULL;

    if ((opt = shift_option(&args, "--test_num=")))
      g_run_multiplier = atoi(opt);
  }

  printf("This test will run %d times.\n",
         ((1 << LG2_CAPACITY) * g_run_multiplier));
  
  // Make sure we have enough cpus.
  // if (tmc_cpus_get_dataplane_cpus(&cpus) != 0)
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("tmc_cpus_get_my_affinity() failed.");
  if (tmc_cpus_count(&cpus) < NUM_THREADS)
    tmc_task_die("Insufficient cpus available.");
    
  // Call the main thread function on each cpu, then wait for them all
  // to exit.
  run_threads(NUM_THREADS, thread_func);
  finish_threads(NUM_THREADS);
  
  return 0;
}
