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

#include "parallel_utils.h"


// --- parallel execution utilities ---

/**
 * Forks/execs n-1 copies of the current command with the same arguments,
 * and affinitizes parent/child processes to the first N available tiles.
 * We assume the command name to exec is in argv[0].
 * Returns index [0 - n-1] in each process (parent or child).
 */
int go_parallel(int n, cpu_set_t* cpus, int argc, char** argv)
{
  int result = 0;
  int watch_forked_children = 0;
  int assume_impending_exec = 0;
  int status;

  if (tmc_task_has_monitor())
  {
    // Give each process its own console streams.
    // (Not strictly necessary, but useful for debugging.)
    status = tmc_task_monitor_console();
    check_tmc_status(status, "tmc_task_monitor_console() failed");
  }

  // check for "child index" flag we'll set below
  const char* index_env_var = "GO_PARALLEL_CHILD_INDEX";
  result = get_int_env(index_env_var, 0);
  if (result == 0)
  {
    // We're the parent process, need to fork/exec children.
    int parent_pid = getpid();

    if (tmc_task_has_shepherd())
    {
      // Let monitor/shepherd know to watch forked children.
      watch_forked_children = tmc_task_watch_forked_children(1);
      check_tmc_status(watch_forked_children,
                       "tmc_task_watch_forked_children() failed");
      assume_impending_exec = tmc_task_assume_impending_exec(1);
      check_tmc_status(assume_impending_exec,
                       "tmc_task_assume_impending_exec() failed");
    }

    printf("Parent (pid=%i): go_parallel(), forking children...\n",
           parent_pid);

    // Fork/exec (n-1) child processes.
    int index;
    for (index = 1; index < n; ++index)
    {

      // Set environment variable with index for exec'd child.
      set_int_env(index_env_var, index);

      // Fork child process.
      pid_t pid = fork();
      if (pid < 0)
      {
        tmc_task_die("go_parallel(): Parent (pid=%i): "
                     "Fork of child %i failed, %s\n",
                     parent_pid, index, strerror(errno));
      }
      else if (pid == 0)
      {
        // NOTE: avoid setting breakpoints in this code block,
        // since the fork()ed child will not have a debugger
        // associated with it until after we exec() below.

        // This is a child process.
        pid = getpid();

        // Export UDN, cmem, etc. state to exec'd process.
        status = tmc_udn_persist_after_exec(1);
        check_tmc_status(status, "tmc_udn_persist_after_exec() failed");
        status = tmc_cmem_persist_after_exec(1);
        check_tmc_status(status, "tmc_cmem_persist_after_exec() failed");

        // Exec copy of executable in this process.
        // This clears any breakpoint-related modifications
        // that were made in the parent process's code.
        execv(argv[0], argv);

        // Normally, the above call should not return.
        tmc_task_die("go_parallel(): Child %i (pid=%i): "
                     "Exec failed, %s\n", index, pid, strerror(errno));
      }
    }

    if (tmc_task_has_shepherd())
    {
      // Let monitor/shepherd know we're done forking.
      status = tmc_task_watch_forked_children(watch_forked_children);
      check_tmc_status(status, "tmc_task_watch_forked_children() failed");
      status = tmc_task_assume_impending_exec(assume_impending_exec);
      check_tmc_status(status, "tmc_task_assume_impending_exec() failed");
    }
  }

  // Affinitize parent/child processes to first N available cpus.
  status = affinitize_to_nth_cpu(result, cpus);
  check_tmc_status(status, "go_parallel(): affinitize_to_nth_cpu() failed");

  return result;
}

/**
 * Affinitizes calling process to the Nth available cpu
 * in the current affinity set (avoiding dedicated tiles, etc.).
 */
int affinitize_to_nth_cpu(int n, cpu_set_t* cpus)
{
  return tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(cpus, n));
}


// --- parent/child persistence utilities ---

/**
 * Detects whether we're the parent or a child exec'd process,
 * using an environment variable.
 */
int is_parent_process(void)
{
  char* is_child_env_var = "IS_PARENT_PROCESS";
  int is_parent = get_int_env(is_child_env_var, 1);
  if (is_parent) set_int_env(is_child_env_var, 0);
  return is_parent;
}

/** Persists integer in environment variable */
void set_int_env(const char* env_var, int value)
{
  const int maxlen = 32;
  char env_var_buf[maxlen];
  snprintf(env_var_buf, maxlen, "%i", value);
  setenv(env_var, env_var_buf, 1);
}

/**
 * Gets integer from environment variable.
 * Returns default value if variable is not defined.
 */
int get_int_env(const char* env_var, int default_value)
{
  int value = default_value;
  char* env_var_val = getenv(env_var);
  if (env_var_val != NULL)
    sscanf(env_var_val, "%i", &value);
  return value;
}

/**
 * Shares int value between parent and children
 * in a fork/exec-based application,
 * using specified environment variable.
 * Returns persisted/retrieved value.
 */
int share_int(const char* env_var, int* value)
{
  char* env_var_val = getenv(env_var);
  if (env_var_val == NULL) {
    // We're the parent process, persist the value.
    const int maxlen = 32;
    char env_var_buf[maxlen];
    snprintf(env_var_buf, maxlen, "%i", *value);
    setenv(env_var, env_var_buf, 1);
  }
  else {
    // We're an exec'd child, get the value.
    *value = 0;
    sscanf(env_var_val, "%i", value);
  }

  return *value;
}

/**
 * Shares pointer value between parent and children
 * in a fork/exec-based application,
 * using specified environment variable.
 * Returns persisted/retrieved value.
 */
void* share_pointer(const char* env_var, void** pointer)
{
  char* env_var_val = getenv(env_var);
  if (env_var_val == NULL) {
    // We're the parent process, persist the value.
    const int maxlen = 32;
    char env_var_buf[maxlen];
    snprintf(env_var_buf, maxlen, "%p", *pointer);
    setenv(env_var, env_var_buf, 1);
  }
  else {
    // We're an exec'd child, get the value.
    *pointer = NULL;
    sscanf(env_var_val, "%p", pointer);
  }

  return *pointer;
}


// --- simple UDN messaging utilities ---

/**
 * Initializes the UDN for the specified number of CPUs,
 * using the first N available CPUs in the application's affinity set.
 */
int udn_init(int n, cpu_set_t* cpus)
{
  // Get the first N available CPUs (skipping any dedicated tiles, etc.).
  cpu_set_t udn_cpu_set;
  tmc_cpus_clear(&udn_cpu_set);
  int i;
  for (i = 0; i < n; i++)
    tmc_cpus_add_cpu(&udn_cpu_set, tmc_cpus_find_nth_cpu(cpus, i));
  return tmc_udn_init(&udn_cpu_set);
}

/** Sends an integer over the UDN to the Nth cpu in the set */
void udn_send_to_nth_cpu(int n, cpu_set_t* cpus, int value)
{
  int cpu = tmc_cpus_find_nth_cpu(cpus, n);
  DynamicHeader to_header = tmc_udn_header_from_cpu(cpu);
  tmc_udn_send_1(to_header, UDN0_DEMUX_TAG, value);
}

/** Receives an integer over the UDN */
int udn_receive(void)
{
  uint32_t received = tmc_udn0_receive();
  return (int) received;
}


// --- error-handling utilities ---

/**
 * Handles possible system error from preceding tmc function call.
 * If err is <0, displays errno error message and exits.
 * If err is 0, does nothing and simply returns.
 */
void check_tmc_status(int status, const char *where)
{
  if (status < 0)
    tmc_task_die("%s: %s", where, strerror(errno));
}
