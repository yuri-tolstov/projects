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

#include <stdio.h>    // printf() etc.
#include <stdlib.h>   // atoi(), getenv(), setenv()
#include <unistd.h>   // fork(), execv()
#include <tmc/task.h> // tmc task utilities
#include <tmc/cpus.h> // tmc cpu_set utilities
#include <tmc/udn.h>  // tmc udn communication utilities
#include <tmc/cmem.h> // tmc common memory utilities
#include <tmc/sync.h> // tmc synchronization utilities
#include <errno.h>    // errno
#include <string.h>   // strerror


// --- parallel execution utilities ---

/**
 * Forks/execs n-1 copies of the current command with the same arguments,
 * and affinitizes parent/child processes to the first N available tiles.
 * We assume the command name to exec is in argv[0].
 * Returns index [0 - n-1] in each process (parent or child).
 */
int go_parallel(int n, cpu_set_t* cpus, int argc, char** argv);

/**
 * Affinitizes calling process to the Nth available cpu
 * in the current affinity set (avoiding dedicated tiles, etc.).
 */
int affinitize_to_nth_cpu(int n, cpu_set_t* cpus);


// --- parent/child persistence utilities ---

/**
 * Detects whether we're the parent or a child process,
 * using an environment variable.
 */
int is_parent_process(void);

/** Persists integer in environment variable. */
void set_int_env(const char* env_var, int value);

/**
 * Gets integer from environment variable.
 * Returns default value if variable is not defined.
 */
int get_int_env(const char* env_var, int default_value);

/**
 * Shares int value between parent and children
 * in a fork/exec-based application,
 * using specified environment variable.
 * Returns persisted/retrieved value.
 */
int share_int(const char* env_var, int* value);

/**
 * Shares pointer value between parent and children
 * in a fork/exec-based application,
 * using specified environment variable.
 * Returns persisted/retrieved value.
 */
void* share_pointer(const char* env_var, void** pointer);


// --- simple UDN messaging utilities ---

/**
 * Initializes the UDN for the specified number of CPUs,
 * starting with CPU 0, from the current affinity set.
 * Note: you should call this function before any operations
 * that restrict the affinity set (like locking processes to tiles).
 */
int udn_init(int num_cpus, cpu_set_t* cpus);

/** Sends an integer over the UDN to the specified cpu. */
void udn_send_to_nth_cpu(int n, cpu_set_t* cpus, int value);

/** Receives an integer over the UDN */
int udn_receive(void);


// --- error-handling utilities ---

/**
 * Handles possible system error from preceding tmc function call.
 * If err is <0, displays errno error message and exits.
 * If err is 0, does nothing and simply returns.
 */
void check_tmc_status(int status, const char *where);
