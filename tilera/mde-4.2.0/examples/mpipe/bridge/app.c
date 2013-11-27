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

#include "app.h"

#define HASH_ORDER 18
#define HASH_ENTRIES (1 << HASH_ORDER)
#define HASH_IDX_MASK (HASH_ENTRIES - 1)

#define AGE_MASK 0xff

//#define SWAP_SMAC_DMAC
#define IGNORE_ALREADY_INSTALLED_ERROR
//#define NO_MULTICAST
//#define NO_BROADCAST
#define SMAC_LOOKUP_HYSTERESIS 
#define CLASSIFIER_PROVIDES_MAC
#define CLASSIFIER_PROVIDES_HASH
//#define EXTRA_CRC_FOR_HASH

// Debug modes are helpful for finding app bugs.
#define DEBUG 0

#define APP_STATS

// macro to wrap a boolean with __builtin_expect
#define likely(cond) __builtin_expect((cond), 1)
#define unlikely(cond) __builtin_expect((cond), 0)

// There are 2 magic MAC values:
// 0x0000000000000000 : Invalid Entry (OUI-0 is reserved according to RFC5342)
// 0x0000000000000001 : Dead Entry (uses broadcast bit)
//
// Dead entries may be reused but are still skipped over when collisions occur.
//
#define DEAD_HASH_MAC 0x0000000000000001
#define INVALID_HASH_MAC 0x0000000000000000
typedef union
{
  struct
  {
    // Word0
    // This includes 16 bits of hash provided by classifier.
    uint_reg_t mac   : 64; 

    // Word1
    uint_reg_t port  : 4;
    uint_reg_t rsvd1 : 4;
    
    uint_reg_t vlan  : 16;

    uint_reg_t rsvd2 : 32;

    uint_reg_t age   : 8;
    
  };

  uint64_t word0;
  uint64_t word1;
} hash_entry_t;

static hash_entry_t *hash_table;

static volatile unsigned long hash_lookups = 0;
static volatile unsigned long lookup_cycles = 0;
static volatile unsigned long multicast_packet_count = 0;
static volatile unsigned int curr_global_age = 0;
static volatile unsigned long mcst_occurred = 0;
static volatile unsigned long dis_src_mac_lookup = 0;
static tmc_spin_mutex_t worker_mutex;

static volatile int hash_valid_count = 0;

#if DEBUG > 0
volatile static int mnum = 100;
#endif


void hash_init()
{

  for (int i = 0; i < HASH_ENTRIES; i++)
  {
    hash_table[i].word0 = 0;
    hash_table[i].word1 = 0;
  }
#if DEBUG > 0
  printf("Initialized %d entry hash table at 0x%lx\n", 
         HASH_ENTRIES, (uint64_t)hash_table);
#endif
}

void app_init(int num_workers)
{
  tmc_alloc_t allocp = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&allocp);

  void* hash_table_page = tmc_alloc_map(&allocp, 
                                        HASH_ENTRIES * sizeof(hash_entry_t));
  assert(hash_table_page);
  hash_table = (hash_entry_t *)hash_table_page;

  tmc_spin_mutex_init(&worker_mutex);

  hash_init();
}

static __USUALLY_INLINE int
hash_mac_address(uint64_t mac_addr)
{
#ifdef CLASSIFIER_PROVIDES_HASH
  return mac_addr >> (64 - HASH_ORDER);
#else
  // turn 48 bit mac address into 24 bits  
  uint64_t mac_hash = mac_addr;
  mac_hash = __insn_crc32_32(0,mac_hash);
  mac_hash = __insn_crc32_32(mac_hash,(mac_addr >> 32));
  // one extra round of bit mixing helps get better hash of MAC MSBs
#ifdef EXTRA_CRC_FOR_HASH
  mac_hash = __insn_crc32_32(mac_hash,0);
#endif
  return mac_hash & HASH_IDX_MASK;
#endif
}

static __USUALLY_INLINE int
is_dead(hash_entry_t *entry)
{
  return entry->mac == DEAD_HASH_MAC;
}

// This returns true if entry is valid or dead.
static __USUALLY_INLINE int
is_valid(hash_entry_t *entry)
{
  return entry->mac != INVALID_HASH_MAC;
}

void hash_stats()
{
  int vld_count = 0;
  int dead_count = 0;
  int port_count[32];
  int collisions = 0;
  unsigned long int total_distance = 0;
  unsigned long int curr_distance = 0;
  unsigned long int max_dist = 0;
  unsigned long int min_dist = ~0;
  unsigned long int coll_distance = 0;
  unsigned long int min_cl = ~0;
  unsigned long int max_cl = 0;
  for (int i=0; i < 32; i++)
    port_count[i] = 0;
  for (int i = 0; i < HASH_ENTRIES; i++)
  {
    if (is_valid(&(hash_table[i])) &&
        !is_dead(&(hash_table[i])))
    {
#if DEBUG > 0
      if (hash_valid_count < 100)
        printf(" %010d : 0x%012lx -> %d\n", i, 
               (uint64_t)(hash_table[i].mac) & (uint64_t)0xffffffffffff, 
               hash_table[i].port);
#endif
      int tgt = hash_mac_address(hash_table[i].mac);
      int dc;
      if (tgt <= i)
        dc = i - tgt;
      else
        dc = i + HASH_ENTRIES - tgt;

      if (dc < min_cl)
        min_cl = dc;
      if (dc > max_cl)
        max_cl = dc;

      coll_distance += dc;

      if (hash_mac_address(hash_table[i].mac) != i)
        collisions++;

      if (vld_count)
      {
        if (curr_distance < min_dist)
          min_dist = curr_distance;
        if (curr_distance > max_dist)
          max_dist = curr_distance;
      }

      vld_count++;
      port_count[hash_table[i].port]++;

      total_distance += curr_distance;
      curr_distance = 0;
    }
    else
    {
      if (vld_count)
        curr_distance += 1;
      if (is_dead(&(hash_table[i])))
        dead_count++;
    }
  }

  printf("Hash table size %d contains %d entries (%0.4f pct).\n",
         HASH_ENTRIES,
         vld_count, 100 * (float)vld_count / (float)HASH_ENTRIES);
  if (vld_count > 1)
    printf("  Distance between entries MIN:%ld MAX:%ld AVG:%0.2f\n",
           min_dist, max_dist,
           (float)total_distance / (float)(vld_count - 1));

  printf("  %d dead entries. %d collisions. MIN:%ld MAX:%ld AVG:%0.2f\n",
         dead_count, collisions, min_cl, max_cl, 
         (float)coll_distance / (float)vld_count);
  for (int i=0; i < 32; i++)
    if (port_count[i])
      printf("  PORT[%d] : %d\n", i, port_count[i]);

  printf("%ld packets multicast\n", multicast_packet_count);
  
}

void app_stats()
{
  hash_stats();
}

void hash_install(uint64_t mac_addr, int port)
{
#if DEBUG > 0
  int collisions = 0;
#endif
  tmc_spin_mutex_lock(&worker_mutex);

  hash_entry_t new_entry = { .word0 = 0, .word1 = 0 };
  new_entry.port = port;
  new_entry.mac = mac_addr;
  int idx = hash_mac_address(mac_addr);
  int max_count = HASH_IDX_MASK - 1; // always leave one invalid entry
  while (max_count--)
  {
    if (!is_valid(&(hash_table[idx])) ||
        is_dead(&(hash_table[idx])))
    {
      hash_table[idx] = new_entry;
      hash_valid_count++;
      __insn_mf();
      tmc_spin_mutex_unlock(&worker_mutex);
      return;
    }

#if DEBUG > 0
    if (++collisions == 4)
      printf(
  "WARNING: More than 4 collisions for a hash entry may reduce performace.\n");
#endif

    if (hash_table[idx].mac == mac_addr)
    {
      tmc_spin_mutex_unlock(&worker_mutex);
#if DEBUG > 1
      printf("Already installed 0x%012lx\n", mac_addr);
#endif
#ifndef IGNORE_ALREADY_INSTALLED_ERROR
      if (hash_table[idx].port != port)
      {
        tmc_task_die(
   "Already-installed entry port does not match new entry port.\n");
      }
#endif
      return;      
    }
    idx = (idx + 1) & HASH_IDX_MASK;    
  }

  tmc_spin_mutex_unlock(&worker_mutex);

  tmc_task_die("Ran out of hash entries during install\n");
}

static int
hash_age_out(int start_entry, int end_entry)
{
  assert(start_entry < HASH_ENTRIES);
  assert(end_entry < HASH_ENTRIES);

  int age_out_count = 0;
  for (int i = start_entry; i < end_entry; i++)
  {
    if (is_valid(&(hash_table[i])) && 
        !is_dead(&(hash_table[i])) &&
        (((curr_global_age - hash_table[i].age) & AGE_MASK) >= 4))
    {
      tmc_spin_mutex_lock(&worker_mutex);
      // If the NEXT entry is not valid, we can make this one invalid.
      // Otherwise, we need to make this one dead.
      int next_idx = (i + 1) & HASH_IDX_MASK;
      if (!is_valid(&(hash_table[next_idx])))
        hash_table[i].mac = INVALID_HASH_MAC;
      else
        hash_table[i].mac = DEAD_HASH_MAC;
      __insn_mf();
      tmc_spin_mutex_unlock(&worker_mutex);
      hash_valid_count--;
      age_out_count++;
    }
  }
  return age_out_count;
}


void
app_maintanance_operations()
{
  static int old_mcst_occurred = 0;
  static int last_aged_out_count = 0;

  // could make this per-VLAN and also programmable
  curr_global_age = (curr_global_age + 1) & AGE_MASK;
  
  // Age out 1/16th of the table each time to reduce cache thrash.
  int start_entry = (curr_global_age & 0xf) * (HASH_ENTRIES >> 4); 
  int end_entry = start_entry + (HASH_ENTRIES >> 4) - 1;
  int age_out_count = hash_age_out(start_entry, end_entry);

  
#ifdef APP_STATS
  // Print hash stats when we stop multicasting or stop aging out.
  if (age_out_count)
    printf("Aged out %d entries\n", age_out_count);
  else if (last_aged_out_count || 
           (!mcst_occurred && old_mcst_occurred))
    hash_stats();

#endif

  last_aged_out_count = age_out_count;
  old_mcst_occurred = mcst_occurred;
  
  if (mcst_occurred) 
  {
    mcst_occurred = 0;
  }
  else
  {
#ifdef SMAC_LOOKUP_HYSTERESIS
#ifdef APP_STATS
    if (!dis_src_mac_lookup)
    {
#if DEBUG > 0
      mnum = 100;
#endif
      printf("Disabling SRC_MAC lookup until next mcst\n");
    }
#endif
    dis_src_mac_lookup = 1;
#endif      
  }

}

static __USUALLY_INLINE uint64_t
get_dmac(gxio_mpipe_idesc_t *idesc)
{
#ifdef CLASSIFIER_PROVIDES_MAC
  // Note that this includs 2 bytes of hashed MAC in the MSBs.
  return idesc->custom2;
#else
  void *pkt_va = gxio_mpipe_idesc_get_va(idesc);
  uint64_t wd0 = ((uint64_t *)pkt_va)[0];
  return wd0 >> 16;
#endif
}

static __USUALLY_INLINE uint64_t
get_smac(gxio_mpipe_idesc_t *idesc)
{
#ifdef CLASSIFIER_PROVIDES_MAC
  // Note that this includs 2 bytes of hashed MAC in the MSBs.
  return idesc->custom3; 
#else
  void *pkt_va = gxio_mpipe_idesc_get_va(idesc);
  pkt_va -= 2;
  uint64_t wd1 = ((uint64_t *)pkt_va)[1];
  return = wd1 & 0xffffffffffff;
#endif
}

// Lookup the hash table, return entry and return NULL if no match.
// The hash table is not allowed to be completely full so this will never hang.
// ISSUE: We don't want to pull a lock for each lookup. But if an age-out
//        occurs followed by an install, the entry could change while it's
//        being accessed.  This could be addressed by considerring the order
//        of accesses when doing an age-out and install.
static __USUALLY_INLINE hash_entry_t*
hash_lookup(uint64_t mac_addr)
{
  int idx = hash_mac_address(mac_addr);
  while (1)
  {
    if (likely(hash_table[idx].mac == mac_addr))
      return &(hash_table[idx]);

    if (unlikely(!is_valid(&(hash_table[idx]))))
      return NULL;

    idx = (idx + 1) & HASH_IDX_MASK;    
  }
}

static __USUALLY_INLINE void
handle_prefetch(gxio_mpipe_idesc_t *idesc)
{
#ifndef CLASSIFIER_PROVIDES_MAC
  void *pkt_va = gxio_mpipe_idesc_get_va(idesc);
  if (!pkt_va) // null buffer
    return;
#endif

  uint64_t dmac = get_dmac(idesc);
  __insn_prefetch(&(hash_table[hash_mac_address(dmac)]));

#ifndef SMAC_LOOKUP_HYSTERESIS
  uint64_t smac = get_smac(idesc);;
  __insn_prefetch(&(hash_table[hash_mac_address(smac)]));
#endif
}

// This is the user application code that runs per-packet.
// Its job is to return the output queue for the packet or DROP_QUEUE if the 
// packet should be dropped.
uint32_t
app_handle_packet(gxio_mpipe_idesc_t *idesc, int input_port)
{
  // We pad the end of the ring so it's safe to prefetch off the end.
  // This saves 1% performance since we don't need to do this check
  // and we don't need to pass in "num_valid".
  //  if (likely(num_valid >= 5))
  handle_prefetch(idesc + 5);

  uint32_t oport_mask = 0;
  
#ifndef CLASSIFIER_PROVIDES_MAC
  __insn_prefetch(gxio_mpipe_idesc_get_va(idesc+2));
  
  // We need to check for packet error since we're going to chase the VA.
  void *pkt_va = gxio_mpipe_idesc_get_va(idesc);
  if (!pkt_va) 
    return DROP_QUEUE;
#endif

  uint64_t dmac = get_dmac(idesc);

#ifdef NO_BROADCAST
  int broadcast = 0;
#else 
  int broadcast = dmac & 1; 
#endif

  hash_entry_t *match_entry =  hash_lookup(dmac);
  
  if (unlikely(broadcast || !match_entry))
  {
#ifdef APP_STATS
    atomic_add(&multicast_packet_count, 1);
#endif

    // It's okay to not do atomics here - if we keep mcsting, it will keep 
    // (re)setting these flags.
    mcst_occurred = 1; 
    dis_src_mac_lookup = 0;

#ifdef NO_MULTICAST
    oport_mask = 0;
#else
    // Multicast to all ports except input port.  
    // The caller will mask off unused queues.
    oport_mask = 0xffffffff;
    oport_mask ^= (1 << input_port);
#if DEBUG > 0
    if (mnum == 1)
    {
      printf("Further MCAST/BCST messages are supressed\n");
      mnum=0;
    }
    else if (mnum)
    {
      atomic_add(&mnum, -1);
      if (broadcast)
        printf(
   "Broadcast (e.g. 0x%012lx) from input %d to port mask 0x%x\n", 
               dmac & 0xffffffffffff,
               input_port, 
               oport_mask);
      else
        printf(
   "Multicasting S: 0x%012lx D: 0x%012lx from input %d to port mask 0x%x\n", 
               get_smac(idesc) & 0xffffffffffff,
               dmac & 0xffffffffffff,
               input_port, 
               oport_mask);
    }
#endif
#endif
  }
  else
  {
    oport_mask = 1 << match_entry->port;

    if (unlikely(match_entry->age != curr_global_age))
      match_entry->age = curr_global_age;
  }
  
#ifdef SMAC_LOOKUP_HYSTERESIS
  int lookup_smac = !dis_src_mac_lookup && !broadcast;
#else
  int lookup_smac = !broadcast;
#endif 

  if (unlikely(lookup_smac))
  {
    uint64_t smac = get_smac(idesc);

    hash_entry_t *match_entry = hash_lookup(smac);

    if (!match_entry)
      hash_install(smac, input_port);
    else if (match_entry->age != curr_global_age)
      match_entry->age = curr_global_age;    
  }

#ifdef SWAP_SMAC_DMAC
  ((uint64_t *)pkt_va)[0] = (smac << 16);
  ((uint64_t *)pkt_va)[1] = dmac | 
    (((uint64_t *)pkt_va)[1] & 0xffff000000000000);
#endif

  return oport_mask;
}


