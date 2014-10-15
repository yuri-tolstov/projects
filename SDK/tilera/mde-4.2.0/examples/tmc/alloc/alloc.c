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
// If an application simply wants page-granularity allocation without
// the overhead of malloc() and without having to deal with mmap(),
// the basic tmc_alloc_map() API is appropriate.  It also provides the
// system-allocator underpinning for mspaces (see <tmc/mspace.h>).
//

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <tmc/alloc.h>
#include <tmc/mspace.h>

int main()
{
  void* p;

  // We'll use "size" for most of our examples as a typical
  // value for allocation (a single page).
  const size_t size = getpagesize();

  // Most interesting uses of tmc_alloc_map() start with an tmc_alloc_t
  // object that describes the characteristics of the memory that is
  // desired, e.g. caching properties, read/write/execute permissions,
  // and memory controller.  The default tmc_alloc_t specifies
  // read/write memory, and uses the MAP_POPULATE flag to prefault the
  // allocated memory to avoid lazy out-of-memory problems.
  //
  // Note that we omit error-checking from this and all following
  // examples for clarity of presentation.
  //
  tmc_alloc_t alloc1 = TMC_ALLOC_INIT;
  p = tmc_alloc_map(&alloc1, size);
  printf("Simple alloc: %p\n", p);

  // When you are done with memory allocated by tmc_alloc_map(), you
  // can free it with tmc_alloc_unmap().  Otherwise, the memory will
  // be "leaked" until exit, as we do in the rest of this example, for
  // clarity of presentation.
  tmc_alloc_unmap(p, size);

  // The tmc_alloc_map() API will round up to a page size boundary
  // (64K by default), so it's necessary to worry about page-size
  // granularity if it's not relevant.
  //
  p = tmc_alloc_map(&alloc1, 1000000);
  printf("Megabyte alloc: %p\n", p);

  // If for some reason memory must be placed somewhere specific,
  // you can use the tmc_alloc_map_at() API to specify the address.
  // This will not overwrite existing mappings (unlike MAP_FIXED)
  // but it will fail and return NULL if that address is not available.
  //
  p = tmc_alloc_map_at(&alloc1, (void*) 0x40000000, size);
  printf("Placement alloc: %p\n", p);

  // Memory allocated with this API can be returned to the 
  // operating system with tmc_alloc_unmap().
  //
  tmc_alloc_unmap(p, size);

  // Normally tmc_alloc_map() allocates default Linux small pages (64K).
  // You can request huge pages instead (16MB).  You must either boot
  // with enough pages specified in the "hugepages" boot argument, or
  // else adjust /proc/sys/mm/hugepages suitably, to ensure enough
  // huge pages are available for your allocation.
  //
  tmc_alloc_t alloc2 = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc2);
  p = tmc_alloc_map(&alloc2, 16 * 1024 * 1024);
  printf("huge page alloc: %p\n", p);

  // The "setter" operations on tmc_alloc_t's can be passed one to
  // another and finally to tmc_alloc_map() to make their use more succinct.
  // Note that the setter modifies its argument even in this case,
  // so subsequent calls will continue to see the modified allocibute.
  //
  tmc_alloc_t alloc3 = TMC_ALLOC_INIT;
  p = tmc_alloc_map(tmc_alloc_set_huge(&alloc3), 16 * 1024 * 1024);
  printf("huge page alloc, method two: %p\n", p);

  // You can control the caching properties of allocations by
  // explicitly requesting which cpu's cache is the home cache for the
  // allocation.  You can also control which memory controller the
  // allocation should take the memory from.  There are TMC_ALLOC_INIT_xxx
  // variant macros that take arguments for initializing an
  // tmc_alloc_t as well; see the <tmc/alloc.h> header for details.
  //
  tmc_alloc_t alloc4 = TMC_ALLOC_INIT;
  tmc_alloc_set_home(&alloc4, 3);              // cache on cpu 3
  tmc_alloc_set_node_preferred(&alloc4, 0);    // memory from controller 0
  p = tmc_alloc_map(&alloc4, size);
  printf("home 3, controller 0 alloc: %p\n", p);

  // You can also control whether the allocation caches locally on
  // individual tiles or not.
  //
  tmc_alloc_set_caching(&alloc4, MAP_CACHE_NO_L1);
  p = tmc_alloc_map(&alloc4, size);
  printf("no level-one caching alloc: %p\n", p);

  // Various TMC_ALLOC_HOME_xxx flags can be passed to tmc_alloc_set_home()
  // instead of a cpu number; these flags allow you to specify memory
  // that is allocated on the current cpu, that follows the current
  // thread while it migrates, that is hash-for-home, or that is
  // incoherent.  Again, see <tmc/alloc.h>.
  //
  tmc_alloc_set_home(&alloc4, TMC_ALLOC_HOME_TASK);
  p = tmc_alloc_map(&alloc4, size);
  printf("task alloc: %p\n", p);

  // An mspace can be requested using memory allocated via an
  // tmc_alloc_t, to provide a convenient sub-page allocation API.
  // See <tmc/mspace.h> for the other mspace-allocation functions
  // (tmc_mspace_malloc, tmc_mspace_calloc, tmc_mspace_free, etc.).
  //
  tmc_mspace msp = tmc_mspace_create_special(0, 0, &alloc4);
  p = tmc_mspace_malloc(msp, 100);
  printf("malloc from tmc_alloc_t heap: %p\n", p);
  tmc_mspace_free(p);

  // When you are done with an mspace, you can return all of its
  // memory back to the system via tmc_mspace_destroy().  Note that
  // this implicitly frees all memory allocated from that mspace.
  //
  tmc_mspace_destroy(msp);

  return 0;
}
