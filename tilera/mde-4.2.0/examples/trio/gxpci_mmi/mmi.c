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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arch/sim.h>
#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/cpus.h>
#include <tmc/alloc.h>
#include <tmc/task.h>
#include <tmc/spin.h>
#include <tmc/ipi.h>
#include <tmc/interrupt.h>


// Trigger via PCIe packets which fall into MAP_MEM_INTx,
// otherwise tile-side MMIO access is used. 
#define PCIE_PKT_TRIGGER

#ifdef PCIE_PKT_TRIGGER
// Memory offset to keep the ready flag.
#define READY_OFFSET    (0x100)
#endif // PCIE_PKT_TRIGGER

// Debug flag
#define DEBUG           (1)

// Number of threads.
#define NUM_THREADS     (1 + 2)

// MACs.
#define SIM0_MAC        (0)
#define SIM1_MAC        (1)

// The TRIO index.
int trio_index = 0;

// Cpu number arrays.
unsigned int cpu_num[NUM_THREADS] = { 0 };

// Memory map region arrays.
int mem_map_index[NUM_THREADS - 1];
void* backing_mems[NUM_THREADS - 1];

// PIO region arrays.
int pio_region_index[NUM_THREADS - 1];
void* mmio_addrs[NUM_THREADS - 1];

// Dummy PCIe address for different MAP_MEM regions.
unsigned long long dummy_pci_bus_addrs[NUM_THREADS] = { 
  0xdeadbeef0000ULL, 
  0xcafebad00000ULL,
};

// IPI events to be sent.
unsigned int ipi_events[NUM_THREADS] = { 0, 31 };

// Flags to be set in IPI C_handler.
int64_t g_ipi_flags[NUM_THREADS] = { 0 };

int __thread rank;
int g_mac;

pthread_t threads[NUM_THREADS];
cpu_set_t cpus;
tmc_spin_barrier_t barrier;


// Parses command line arguments in order to fill in the MAC.
void 
parse_args(int argc, char** argv)
{
  if (argc < 2)
    tmc_task_die("Usage: mmi sim_number");
  int sim = atoi(argv[1]);
  
  if (sim == 0)
  {
    g_mac = SIM0_MAC;
  }
  else
  {
    g_mac = SIM1_MAC;
  }
}

// User defined C_handler.
void 
on_ipi_event(void* arg)
{
  // Setup per thread flag to the master tile.
  g_ipi_flags[rank] = 1;
}

// Thread func
//
void* 
thread_func(void* arg)
{
  rank = (intptr_t)arg;

  // Bind this thread to the rank'th core in the cpu set.
  int cpu = tmc_cpus_find_nth_cpu(&cpus, rank);
  if (tmc_cpus_set_my_cpu(cpu) < 0)
    tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");

  // Now that we're bound to a core, activate IPI.
  if (tmc_ipi_activate() < 0)
    tmc_task_die("Failure in 'tmc_ipi_activate()'.");

  // Record all the cpu numbers into the global array.
  cpu_num[rank] = cpu;

  tmc_spin_barrier_wait(&barrier);
  
#if DEBUG
  printf("MAC%d: rank %d (cpu %d) starts its thread\n", 
         g_mac, rank, cpu_num[rank]);
#endif // DEBUG 

  if (rank == 0)
  {
    // Setup and init TRIO and we have already bound to a single cpu here.
    gxio_trio_context_t trio_context_body;
    gxio_trio_context_t* trio_context = &trio_context_body;
    gxpci_context_t gxpci_context_body;
    gxpci_context_t* gxpci_context = &gxpci_context_body;

    // Get a gxio context.
    int trio_index = 0;

    int result = gxio_trio_init(trio_context, trio_index);
    if (result != 0)
      tmc_task_die("Failure in 'gxio_trio_init()'.");

    result = gxpci_init(trio_context, gxpci_context, trio_index, g_mac);
    if (result != 0)
      tmc_task_die("Failure in 'gxio_init()'.");

    // Allocate an ASID if it isn't pre-allocated.
    int asid = gxio_trio_alloc_asids(trio_context, 1, 0, 0);
    if (asid < 0)
      tmc_task_die("Failure in 'gxio_trio_alloc_asids()'.");
    
    gxpci_context->resource.asid = asid;
  
    // TRIO resource init for all the target tiles. 
    for (int i = 0; i < NUM_THREADS - 1; i++)
    {
      // Allocate and bind a page to memory map region.
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      backing_mems[i] = tmc_alloc_map(&alloc, getpagesize());
      if (backing_mems[i] == NULL)
        tmc_task_die("Failure in 'tmc_alloc_map()'.");
    
      int err = gxio_trio_register_page(trio_context, asid, backing_mems[i],
                                        getpagesize(), 0);
      if (err != 0)
        tmc_task_die("Failure in 'gxio_trio_register_page()'.");

      // Allocate a memory map region.
      mem_map_index[i] = gxio_trio_alloc_memory_maps(trio_context, 1, 0, 0);
      if (mem_map_index[i] < 0)
        tmc_task_die("Failure in 'gxio_trio_alloc_memory_maps()'.");

      // Init memory map region.
      uint64_t offset = g_mac * getpagesize();
      err = gxio_trio_init_memory_map(trio_context, mem_map_index[i],
                                      backing_mems[i], getpagesize(), asid, 
                                      g_mac, dummy_pci_bus_addrs[i] + offset,
                                      GXIO_TRIO_ORDER_MODE_STRICT);
      if (err != 0)
        tmc_task_die("Failure in 'gxio_trio_init_memory_map()'.");

      // Allocate a PIO region. 
      pio_region_index[i] = gxio_trio_alloc_pio_regions(trio_context, 1, 0, 0);
      if (pio_region_index[i] < 0)
        tmc_task_die("Failure in 'gxio_trio_alloc_pio_regions()'.");

      // Target to different PCIe addresses.
      offset = (g_mac == SIM0_MAC) ? SIM1_MAC : SIM0_MAC;
      offset *= getpagesize();
      err = gxio_trio_init_pio_region(trio_context, pio_region_index[i], g_mac,
                                      dummy_pci_bus_addrs[i] + offset, 0);
      if (err != 0)
        tmc_task_die("Failure in 'gxio_trio_init_pio_region()'.");
 
      // MMIO map to PIO region.
      mmio_addrs[i] = gxio_trio_map_pio_region(trio_context, 
                                               pio_region_index[i],
                                               getpagesize(), 0);
      if (mmio_addrs[i] == MAP_FAILED)
        tmc_task_die("Failure in gxio_trio_map_pio_region: MAP_FAILED: %s.",
                     gxio_strerror(-errno));

      // Enable memory map interrupt for a particular memory map region which
      // has already been allocated.
      // NOTE: level mode is not supported by simulator.
      err = gxio_trio_enable_mmi(trio_context, cpu_num[i + 1], ipi_events[i],
                                 mem_map_index[i],
                                 TRIO_MAP_MEM_SETUP__INT_MODE_VAL_EDGE);
      if (err != 0)
        tmc_task_die("Failure in 'gxio_trio_enable_mmi()'.");
    }

    // 1st sync to wait for target tile IPI interrupts' installation.
    tmc_spin_barrier_wait(&barrier);

#ifdef PCIE_PKT_TRIGGER
    // Setup the ready flag which is checked by the other side.
    *((uint8_t*)backing_mems[0] + READY_OFFSET) = 1;
    __insn_mf();

    // Check the ready flag. 
    while (1) 
    {
      if (gxio_trio_read_uint64(mmio_addrs[0] + READY_OFFSET) == 1)
	break;
    }
#endif // PCIE_PKT_TRIGGER 

    // Trigger memory map interrupts.
    for (int i = 0; i < NUM_THREADS - 1; i++)
    {
      // Trigger the IPI interrupts on TILE side via MMIO.
#ifndef PCIE_PKT_TRIGGER
      int err = gxio_trio_write_mmi_bits(trio_context, mem_map_index[i], 
                                         TRIO_MAP_MEM_REG_INT0__INT_VEC_MASK, 
                                         GXIO_TRIO_MMI_BITS_WRITE);
      if (err != 0)
        tmc_task_die("Failure in 'gxio_trio_write_mmi_bits()'.");
#else // Trigger the IPI interrupts by other PCIe device.
      // Send PIO packet to receiver tile's MAP_MEM_INT0 register. 
      gxio_trio_write_uint64(mmio_addrs[i], 
                             TRIO_MAP_MEM_REG_INT0__INT_VEC_MASK - g_mac);
      printf("MAC%d: PIO writes to %p\n", g_mac, mmio_addrs[i]);
#endif // PCIE_PKT_TRIGGER
    }

    // 2nd sync to wait for target tile IPI interrupts' completion.
    tmc_spin_barrier_wait(&barrier);
    
    for (int i = 0; i < NUM_THREADS - 1; i++)
    {
      int bits = gxio_trio_read_mmi_bits(trio_context, mem_map_index[i]);
#if DEBUG
      printf("MAC%d: Interrupt status for MAP_MEM region%d is %x\n", 
             g_mac, mem_map_index[i], bits);
#endif // DEBUG

      // Use W1TC register to clear the pending status.
      gxio_trio_write_mmi_bits(trio_context, mem_map_index[i], bits, 
                               GXIO_TRIO_MMI_BITS_W1TC);
#if DEBUG
      bits = gxio_trio_read_mmi_bits(trio_context, mem_map_index[i]);
      
      printf("MAC%d: Interrupt status for MAP_MEM region%d is %x\n", 
             g_mac, mem_map_index[i], bits);
#endif // DEBUG
    }
  }
  else
  {
    // Init and unmask IPI interrupt.
    tmc_ipi_event_install(0, ipi_events[rank - 1], on_ipi_event,
                          (void*)(long)ipi_events[rank - 1]);

    // 1st sync.    
    tmc_spin_barrier_wait(&barrier);

    // Check flags and clear pending MAP_MEM interrupts.
    while (g_ipi_flags[rank] != 1)
      arch_atomic_compiler_barrier();
    
    // 2nd sync.
    tmc_spin_barrier_wait(&barrier);
  }

  return (void*)NULL;
}

// Function to create all threads.
//
void 
run_threads(int count, void*(*func)(void*))
{
  for (int i = 0; i < count; i++)
  {
    if (pthread_create(&threads[i], NULL, func, (void*)(intptr_t)i) != 0)
      tmc_task_die("pthread_create() failed.");
  }
}

// Function to collect all threads.
//
void 
finish_threads(int count)
{
  for (int i = 0; i < count; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("pthread_join() failed.");
  }
}

int main(int argc, char**argv)
{
  // Parse the inputs.
  parse_args(argc, argv);

  // Make sure we have enough cpus.
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");

  if (tmc_cpus_count(&cpus) < NUM_THREADS)
    tmc_task_die("Insufficient cpus available."); 

  // Initialize this process for IPI use.
  if (tmc_ipi_init(NULL) < 0)
    tmc_task_die("Failure in 'tmc_ipi_init()'.");

  // Barrier init.
  tmc_spin_barrier_init(&barrier, NUM_THREADS);

  // Go parallel.
  run_threads(NUM_THREADS, thread_func);

  // Wait for all threads.
  finish_threads(NUM_THREADS);

  return 0;
}
