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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <tmc/alloc.h>

#include <arch/cycle.h>
#include <arch/spr.h>

#include <tmc/cpus.h>
#include <tmc/sync.h>
#include <tmc/task.h>


#define NUM_THREADS 32
pthread_t threads[NUM_THREADS];
cpu_set_t cpus;

// Used to communicate buffer addresses between threads.
uint8_t* buffers[2];

// Barrier objects for synchronizing 2 threads or NUM_THREADS threads.
tmc_sync_barrier_t barrier_two = TMC_SYNC_BARRIER_INIT(2);
tmc_sync_barrier_t barrier_all = TMC_SYNC_BARRIER_INIT(NUM_THREADS);


// Measure the average load latency to a particular memory address.
// This routine will stride through the specified memory buffer,
// performing a load every 'stride' bytes and wrapping after 'bound'
// bytes.  'bound' must be a power of two.
//
uint64_t measure_load_latency(volatile uint8_t* base, size_t stride,
                              size_t bound)
{
  const int ITERATIONS = 10000000;
  size_t mask = (bound - 1);
  size_t offset = 0;
	
  uint64_t start = get_cycle_count();
  for (int i = 0; i < ITERATIONS; i++)
  {
    // Load into a useless SPR so that the compiler won't drop the
    // load or do any interesting scheduling.  We chose this SPR
    // because it accessible to userspace and has a single-cycle
    // access time.
    __insn_mtspr(SPR_SYSTEM_SAVE_0_0, base[offset]);
    offset = (offset + stride) & mask;
  }
  uint64_t finish = get_cycle_count();
	
  // Calculate the average load latency.  We know that the compiler 
  // has to schedule the load, mtspr, and branch operation in separate
  // bundles, and we want to measure how many cycles were consumed in 
  // addition to those bundles, so we subtract three from the average
  // loop iteration time.
  return (finish - start) / ITERATIONS - 3;
}


// Compare the load latency of uncacheable memory on loaded
// vs. unloaded memory controllers.  We allocate uncacheable memory so
// that all load accesses will miss in the cache and go to DRAM.
//
// This method should be invoked by NUM_THREADS threads in parallel.
// One of them will measure the latency of shim 0, and the other
// (NUM_THREADS - 1) will access shim 1 simultaneously, resulting in
// significantly higher load.
//
void numa_example(int rank)
{
  // Allocate an uncacheable page on memory controllers 0 and 1.
  tmc_alloc_t node0 = TMC_ALLOC_INIT;
  tmc_alloc_set_home(&node0, TMC_ALLOC_HOME_NONE);
  tmc_alloc_set_node_preferred(&node0, 0); 
  tmc_alloc_t node1 = TMC_ALLOC_INIT;
  tmc_alloc_set_home(&node1, TMC_ALLOC_HOME_NONE);
  tmc_alloc_set_node_preferred(&node1, 1); 
  if (rank == 0)
  {
    buffers[0] = tmc_alloc_map(&node0, sizeof(uint32_t));
    if (buffers[0] == NULL)
      tmc_task_die("Failed to allocate node0 memory.");
    buffers[1] = tmc_alloc_map(&node1, sizeof(uint32_t));
    if (buffers[1] == NULL)
      tmc_task_die("Failed to allocate node1 memory.");
  }
	
  // Synchronize so all threads see buffer addresses.
  tmc_sync_barrier_wait(&barrier_all);
		
  // Compare access times for node 0 vs. node 1 when one thread
  // is accessing 0 and 31 are accessing 1.
  uint64_t latency;
  if (rank == 0)
    latency = measure_load_latency(buffers[0], 0, 0);
  else
    latency = measure_load_latency(buffers[1], 0, 0);
		
  // Synchronize and print results.
  tmc_sync_barrier_wait(&barrier_all);
  if (rank == 0)
  {
    printf("numa_example():\n");
    printf("  Node 0 (unloaded) latency = %lld\n", (long long)latency);
  }
  tmc_sync_barrier_wait(&barrier_all);
  if (rank == 1)
    printf("  Node 1 (loaded) latency = %lld\n", (long long)latency);
  tmc_sync_barrier_wait(&barrier_all);
	
  // Release the memory.
  if (rank == 0)
  {
    tmc_alloc_unmap(buffers[0], sizeof(uint32_t));
    buffers[0] = NULL;	
    tmc_alloc_unmap(buffers[1], sizeof(uint32_t));
    buffers[1] = NULL;
  }	
  tmc_sync_barrier_wait(&barrier_all);
}


// Compare the latency of memory that is homed on the local tile vs.
// a remote tile.  On TILE64, accessing remote data requires a
// round-trip to the remote cache for every load.  On TILEPro64, the
// cache subsystem makes a local copy of the cacheline on each load,
// so that repeated access to the same cacheline does not require a
// round-trip to the home cache.
//
// This method should be invoked by two threads in parallel.
//
void home_cache_example(int rank)
{	
  // Allocate a page homed on rank 0.
  tmc_alloc_t home0 = TMC_ALLOC_INIT;
  tmc_alloc_set_home(&home0, TMC_ALLOC_HOME_HERE);
  if (rank == 0)
  {
    buffers[0] = tmc_alloc_map(&home0, sizeof(uint32_t));
    if (buffers[0] == NULL)
      tmc_task_die("Failed to allocate rank 0 memory.");
  }
	
  // Synchronize so all threads see buffer addresses.
  tmc_sync_barrier_wait(&barrier_two);
		
  // Compare access times for local vs. remote cpu.
  uint64_t latency = measure_load_latency(buffers[0], 0, 0);
  tmc_sync_barrier_wait(&barrier_two);
	
  // Measure remote access time when contended.  We use a flag 
  // byte to start and stop this phase.
  uint64_t latency_contended = 1;
  volatile uint8_t* data = buffers[0];
  if (rank == 1)
  {
    // Remote tile just measures load latency.
    data[1] = 1; // start
    latency_contended = measure_load_latency(data, 0, 0);
    data[1] = 2; // stop
  }
  else
  {
    // Rank 0 issues stores to the same cacheline.  On TILEPro64, this
    // triggers hardware cache coherence invals, thus increasing the
    // load latency.  On TILE64, the hardware never makes a copy of 
    // the cacheline on the remote cpu, so this has little effect. 
    while (data[1] != 2)
    {
      data[0] = 0;
    }
  }
		
  // Synchronize and print results.
  tmc_sync_barrier_wait(&barrier_two);
  if (rank == 0)
  {
    printf("home_cache_example():\n");
    printf("  Local uncontended load latency = %lld\n", (long long)latency);
  }
  tmc_sync_barrier_wait(&barrier_two);
  if (rank == 1)
    printf("  Remote uncontended load latency = %lld\n", (long long)latency);
  tmc_sync_barrier_wait(&barrier_two);
  if (rank == 1)
    printf("  Remote contended load latency = %lld\n",
           (long long)latency_contended);
  tmc_sync_barrier_wait(&barrier_two);
	
  // Release the memory.
  if (rank == 0)
  {
    tmc_alloc_unmap(buffers[0], sizeof(uint32_t));
    buffers[0] = NULL;
  }	
  tmc_sync_barrier_wait(&barrier_two);
}


// Compare latency for random access data with huge pages and normal
// pages.  When a program accesses large amount of data (many
// megabytes) in a random fashion, the processor's Translation
// Lookaside Buffer (TLB) will not be able to hold translations for
// all of the pages being accessed.  Huge pages perform better in such
// cases because they allow a single TLB entry to span 16 megabytes of
// memory rather than 64 kB.
//
// Applications should limit the use of huge pages to cases where TLB
// misses have a significant performance impact.  Huge pages are a
// limited system resource, tend to waste memory due to fragmentation,
// and must be allocated at system startup time.
//
// This method should be invoked by a single thread.
//
void huge_page_example(int rank)
{	
  // We'll stride through 32MB of each type of memory.
  const size_t BUFFER_SIZE = (32 * 1024 * 1024);
	
  // Allocate a normal page and a huge page.
  tmc_alloc_t normal_alloc = TMC_ALLOC_INIT;
  tmc_alloc_t huge_alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&huge_alloc);
	
  uint8_t* huge = tmc_alloc_map(&huge_alloc, BUFFER_SIZE);
  if (huge == NULL)
    tmc_task_die("Failed to allocate huge page memory.");

  uint8_t* normal = tmc_alloc_map(&normal_alloc, BUFFER_SIZE);
  if (normal == NULL)
    tmc_task_die("Failed to allocate normal memory.");
	
  // Measure average latency when striding through those buffers at 
  // page-size increments.  We expect that all these loads will cache miss, 
  // since we're striding through a large amount of memory touching each line 
  // only once.  The normal page accesses will always take a TLB miss, 
  // whereas the huge pages should never take a TLB miss.
  size_t stride = getpagesize();
  uint64_t huge_latency = measure_load_latency(huge, stride, BUFFER_SIZE);
  uint64_t normal_latency = measure_load_latency(normal, stride, BUFFER_SIZE);
	
  printf("huge_page_example():\n");
  printf("  normal page latency (cache + TLB miss) = %lld\n",
         (long long)normal_latency);
  printf("  huge page latency (cache miss only) = %lld\n",
         (long long)huge_latency);
	
  // Release the memory.
  tmc_alloc_unmap(huge, BUFFER_SIZE);
  tmc_alloc_unmap(normal, BUFFER_SIZE);
}


// The worker function for each thread.  We invoke each memory example
// function in turn and then exit.
//
void* thread_func(void* arg)
{
  int rank = (long)arg;
  
  // Bind this thread to the rank'th core in the cpu set.
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank)) < 0)
    tmc_task_die("tmc_cpus_set_my_cpu() failed.");
    
  numa_example(rank);	
  
  if (rank < 2)
    home_cache_example(rank);

  if (rank == 0)
    huge_page_example(rank);
	      
  tmc_sync_barrier_wait(&barrier_all);
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
  // Make sure we have enough cpus.
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
