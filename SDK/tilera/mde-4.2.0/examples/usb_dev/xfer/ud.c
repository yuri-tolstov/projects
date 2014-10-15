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
//
//
// USB device tile-side example.
//
// WARNING: before using this code as the basis for any production
// application, take special note of the caveats in this example's
// README.txt file.
//

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tmc/cpus.h>
#include <tmc/interrupt.h>
#include <tmc/ipi.h>

#include "ud.h"
#include "util.h"

//
// Command options, first the long versions.
//
static const struct option long_options[] =
{
#ifndef NDEBUG
  { .name = "debug",              .has_arg = 0, .val = 'd' },
#endif
  { .name = "help",               .has_arg = 0, .val = 'h' },
  { 0 },
};

//
// Now the short versions.
//
#ifndef NDEBUG
static const char options[] = "dh";
#else
static const char options[] = "h";
#endif

//
// Option flags.
//
int debug = 0;

/** Name of this program, for error messages. */
const char* myname;

//
// Print a usage message.
//
void
usage(int exstat)
{
  printf(
"usage: ud <options>:\n"
#ifndef NDEBUG
"  --debug              Enable debug output; may be repeated for even more\n"
"                       verbose output.  Note that this will decrease\n"
"                       performance.\n"
#endif
"  --help               Print this message.\n"
  );

  exit(exstat);
}

//
// Main program.
//
int
main(int argc, char** argv)
{
  myname = "ud";

  //
  // Parse our arguments.
  //
  int opt;

  while ((opt = getopt_long(argc, argv, options, long_options, NULL)) > 0)
  {
    switch (opt)
    {
#ifndef NDEBUG
    case 'd':   // --debug
      debug++;
      break;
#endif

    case 'h':  // --help
      usage(0);
      break;

    default:
      //
      // Note that getopt already printed an error message for us.
      //
      usage(1);
      break;
    }
  }

  //
  // We need to bind to a CPU in order to use interrupts.  Note that we
  // don't bother using dataplane mode because we know we're going to be
  // making system calls to read/write data files; in a real application,
  // if we were getting our data from some source other than system calls,
  // we would probably want to enable it.
  //
  int cpu = tmc_cpus_get_my_current_cpu();
  int rv = tmc_cpus_set_my_cpu(cpu);
  if (rv != 0)
    pbomb("set_cpu failed");

  cpu_set_t cpus;
  rv = tmc_cpus_get_my_affinity(&cpus);
  if (rv != 0)
    pbomb("get_affinity failed");

  rv = tmc_ipi_init(&cpus);
  if (rv != 0)
    pbomb("ipi_init failed");

  rv = tmc_ipi_activate();
  if (rv != 0)
    pbomb("ipi_activate failed");

  proc(cpu);
}
