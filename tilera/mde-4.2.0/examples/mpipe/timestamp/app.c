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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <arch/sim.h>
#include <gxio/mpipe.h>
#include <tmc/task.h>


// The mPIPE instance.
static int instance;

// The mpipe context.
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// Help check for errors.
#define VERIFY(VAL, WHAT)                                       \
  do                                                            \
  {                                                             \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

static void usage(char *progname)
{
  fprintf(stderr,
          "usage: %s [options]\n"
          " -d name  device to open\n"
          " -f val   adjust the mPIPE timestamp clock frequency by 'val' ppb,\n"
          "          the absolute value of 'val' must not greater than\n"
          "          1000000000, and should not less then 30000, otherwise\n"
          "          will be ignored because of clock precision restriction\n"
          " -g       get the mPIPE timestamp clock time\n"
          " -h       prints this message\n"
          " -s       set the mPIPE timestamp clock time from the system time\n"
          " -S       set the system time from the mPIPE timestamp clock time\n"
          " -t val   shift the mPIPE timestamp clock time by 'val'\n"
          "          nanoseconds, the absolute value of 'val' must not\n"
          "          greater than 1000000000\n",
          progname);
}

int
main(int argc, char** argv)
{
  char c;
  int ret;
  char* progname;
  int gettime = 0;
  int settime = 0;
  long adjtime = 0;
  long adjfreq = 0;
  char* link_name = "loop0";

  progname = strrchr(argv[0], '/');
  progname = progname ? 1+progname : argv[0];
  while (EOF != (c = getopt(argc, argv, "d:f:ghsSt:")))
  {
    switch (c)
    {
      case 'd':
        link_name = optarg;
        break;
      case 'g':
        gettime = 1;
        break;
      case 's':
        settime = 1;
        break;
      case 'S':
        settime = 2;
        break;
      case 't':
        adjtime = atol(optarg);
        if (adjtime > 1000000000 || adjtime < -1000000000)
          return -1;
        break;
      case 'f':
        adjfreq = atol(optarg);
        if (adjfreq > 1000000000 || adjfreq < -1000000000)
          return -1;
        break;
      case 'h':
        usage(progname);
        return 0;
      default:
        usage(progname);
        return -1;
    }
  }

  if (strncmp(link_name, "loop", 4) != 0)
    tmc_task_die("Must use a 'loop*' link.");

  // Get the instance.
  instance = gxio_mpipe_link_instance(link_name);
  if (instance < 0)
    tmc_task_die("Link '%s' does not exist.", link_name);

  // Start the driver.
  ret = gxio_mpipe_init(context, instance);
  VERIFY(ret, "gxio_mpipe_init()");

  struct timespec ts;
  if (gettime)
  {
    gxio_mpipe_get_timestamp(context, &ts);
    printf("gxio_mpipe_get_timestamp: time=%ld.%ld or %s\n",
           (long)ts.tv_sec, (long)ts.tv_nsec, ctime(&ts.tv_sec));
  }

  struct timeval now;
  if (settime == 1)
  {
    if (gettimeofday(&now, NULL))
    {
      printf("gettimeofday() failed\n");
      return -1;
    }

    ts.tv_sec = now.tv_sec;
    ts.tv_nsec = now.tv_usec * 1000;
    gxio_mpipe_set_timestamp(context, &ts);
    printf("set mPIPE time from system time, time=%ld.%ld or %s\n",
           (long)ts.tv_sec, (long)ts.tv_nsec, ctime(&ts.tv_sec));
  }
  else if (settime == 2)
  {
    gxio_mpipe_get_timestamp(context, &ts);
    now.tv_sec = ts.tv_sec;
    now.tv_usec = ts.tv_nsec / 1000;

    if (settimeofday(&now, NULL))
    {
      printf("settimeofday() failed\n");
      return -1;
    }
    else
      printf("set system time from mPIPE time, time=%ld.%ld or %s\n",
             (long)now.tv_sec, (long)now.tv_usec, ctime(&ts.tv_sec));
  }

  if (adjtime)
  {
    ret = gxio_mpipe_adjust_timestamp(context, adjtime);
    if (ret)
    {
      printf("gxio_mpipe_adjust_timestamp() failed\n");
      return -1;
    }
    else
      printf("adjust mPIPE timestamp clock %ld ns\n", adjtime);
  }

  if (adjfreq)
  {
    ret = gxio_mpipe_adjust_timestamp_freq(context, adjfreq);
    if (ret)
    {
      printf("gxio_mpipe_adjust_timestamp_freq() failed\n");
      return -1;
    }
    else
      printf("adjust mPIPE timestamp clock %ld ppb\n", adjfreq);
  }

  return 0;
}
