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

#include <tmc/cmem.h>
#include <tmc/cpus.h>
#include <tmc/task.h>
#include <tmc/udn.h>


// This example creates two sub-apps, which share a cmem arena and a
// UDN hardwall, but which each use a different executable.
//
int
main(int argc, char**argv)
{
  // Each of the 2 sub-apps will get 3 cpus from the initial affinity.
  size_t total = 2 * 3;

  // Make sure we have sufficient cpus.
  cpu_set_t cpus;
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
  size_t count = tmc_cpus_count(&cpus);
  if (count < total)
    tmc_task_die("Insufficient cpus available.");

  // Split the cpus among the apps.
  cpu_set_t cpus1, cpus2;
  tmc_cpus_clear(&cpus1);
  tmc_cpus_clear(&cpus2);
  for (int i = 0; i < total; i++)
  {
    int cpu = tmc_cpus_find_nth_cpu(&cpus, i);
    tmc_cpus_add_cpu((i % 2) ? &cpus2 : &cpus1, cpu);
  }

  // Discard any extra cpus.
  tmc_cpus_clear(&cpus);
  tmc_cpus_add_cpus(&cpus, &cpus1);
  tmc_cpus_add_cpus(&cpus, &cpus2);

  // Create common memory.
  if (tmc_cmem_init(0) != 0)
    tmc_task_die("Failure in 'tmc_cmem_init()'.");

  // Create a udn hardwall around the cpus.
  if (tmc_udn_init(&cpus) != 0)
    tmc_task_die("Failure in 'tmc_udn_init()'.");

  // The "persist" functions below set up the environment so that the
  // cmem arena and UDN hardwall will persist across execv().  The new
  // executables will call tmc_cmem_init() and tmc_udn_init() to gain
  // access to the persisted environment.  Normally, one would make
  // these calls in the child process, between the fork() and execv()
  // calls, but this example calls execv() in the parent process too.
  tmc_cmem_persist_after_exec(1);
  tmc_udn_persist_after_exec(1);

  // The following function tells the 'shepherd' (if there is one)
  // that forked children should be considered part of the same
  // application.  This allows various task management features, such
  // as debugging, to work properly.  Normally, one would make this
  // call in the child process, between the fork() and execv() calls,
  // but this example calls execv() in the parent process too.
  tmc_task_watch_forked_children(1);

  pid_t child = fork();
  if (child < 0)
    tmc_task_die("Failure in 'fork()'.");

  if (child != 0)
  {
    // Parent.
    tmc_cmem_detach();
    if (tmc_cpus_set_my_affinity(&cpus1) != 0)
      tmc_task_die("Failure in 'tmc_cpus_set_my_affinity()'.");
    argv[0] = "./app1";
    execv(argv[0], argv);
  }
  else
  {
    // Child.
    if (tmc_cpus_set_my_affinity(&cpus2) != 0)
      tmc_task_die("Failure in 'tmc_cpus_set_my_affinity()'.");
    argv[0] = "./app2";
    execv(argv[0], argv);
  }

  tmc_task_die("Failure in 'exec()'.");
}
