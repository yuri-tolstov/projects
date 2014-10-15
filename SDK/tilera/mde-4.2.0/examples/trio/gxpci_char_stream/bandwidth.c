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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/tilegxpci.h>

#include <sys/time.h>


#define ITERATIONS  	5
#define BUFFER_RUNS 	2048

// The host machine has a different tilegxpci device filename than the
// Tile side.
#ifdef __tile__
static const char pattern[] = "/dev/trio0-mac0/%d";
#else
static const char pattern[] = "/dev/tilegxpci%d/%d";
#endif // __tile__

// Only print output on the host side so as to avoid interleaving
// output from the host and tile side programs.
#ifdef __tile__
#define PRINTF(...)
#else
#define PRINTING
#define PRINTF(...) printf(__VA_ARGS__)
#endif

// The character stream index, with valid values from 1 to 3. 
// Note that channel 0 is reserved for tile-monitor and can not be used.
static int chan_idx = 1;

size_t buf_size = 4096;

// The "/dev/tilegxpci%d" device to be used.
unsigned int card_index = 0;

static void
exit_strerror(char* message)
{
  fprintf(stderr, "%s: %s (%d)", message, strerror(errno), errno);
  exit(1);
}


static char *
shift_option(char ***arglist, const char* option)
{
  char** args = *arglist;
  char* arg = args[0], **rest = &args[1];
  int optlen = strlen(option);
  char* val = arg + optlen;
  if (option[optlen - 1] != '=')
  {
    if (strcmp(arg, option))
      return NULL;
  }
  else
  {
    if (strncmp(arg, option, optlen - 1))
      return NULL;
    if (arg[optlen - 1] == '\0')
      val = *rest++;
    else if (arg[optlen - 1] != '=')
      return NULL;
  }
  *arglist = rest;
  return val;
}


// Parses command line arguments in order to fill in the channel index
// and buffer size variables.
static void 
parse_args(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--chan="))) 
    {
      chan_idx = atoi(opt);
      if (chan_idx < 1 || chan_idx >= TILEPCI_NUM_CHAR_STREAMS)
      {
	chan_idx = 1;
        fprintf(stderr, 
                "Illegal character stream index, use default chan 1 instead\n");
      }
    }
    else if ((opt = shift_option(&args, "--size=")))
    {
      buf_size = atoi(opt);
    }
    else if ((opt = shift_option(&args, "--card=")))
    {
      card_index = atoi(opt);
    }
    else
    {
      fprintf(stderr, "Unknown option '%s'.\n", args[0]);
#ifdef __tile__
      fprintf(stderr, "Usage: bandwidth.tile --chan=1 --size=4096\n");
#else
      fprintf(stderr,
              "Usage: ./bandwidth.host --chan=1 --size=4096 --card=0\n");
#endif // __tile__
      exit(EXIT_FAILURE);
    }
  }
}


static int
open_channel(int channel)
{
  char name[128];
  int fd;
#ifdef __tile__
  sprintf(name, pattern, channel);
#else
  sprintf(name, pattern, card_index, channel);
#endif // __tile__

  fd = open(name, O_RDWR);
  if (fd < 0)
  {
    fprintf(stderr, "Failed to open '%s': %s\n", name, strerror(errno));
    exit(EXIT_FAILURE);
  }

  return fd;
}


static void
do_read(int fd, void* buffer, size_t size)
{
  size_t progress = 0;
  do
  {
    ssize_t result = read(fd, buffer + progress, size - progress);
    if (result >= 0)
      progress += result;
    else if (errno != EINTR)
      exit_strerror("read() failed");
  }
  while (progress != size);
}


static void
do_write(int fd, const void* buffer, size_t size)
{
  size_t progress = 0;
  do
  {
    ssize_t result = write(fd, buffer + progress, size - progress);
    if (result >= 0)
      progress += result;
    else if (errno != EINTR)
      exit_strerror("write() failed");
  }
  while (progress != size);
}


void
test_host_to_tile(int fd, void* buffer, size_t buf_size)
{
  PRINTF("bandwidth host-to-TLR test:\n");
  
  for (int i = 0; i < ITERATIONS; i++)
  {
#ifdef PRINTING
    struct timeval before, after;
    gettimeofday(&before, NULL);
#endif

    for (int j = 0; j < BUFFER_RUNS; j++)
    {
#ifdef __tile__
      do_read(fd, buffer, buf_size);
#else
      do_write(fd, buffer, buf_size);
#endif
    }

#ifdef PRINTING
    gettimeofday(&after, NULL);
    double secs = after.tv_sec - before.tv_sec;
    double usecs = after.tv_usec - before.tv_usec;
    double time = secs + usecs * 1e-6;
    
    PRINTF("%f seconds for %ld bytes, %f Mb/s\n",
           time, (unsigned long)(buf_size * BUFFER_RUNS), 
           (buf_size * BUFFER_RUNS * 8) / time / 1e6);
#endif
  }
}


void
test_tile_to_host(int fd, void* buffer, size_t buf_size)
{
  PRINTF("bandwidth TLR-to-host test:\n");
    
  for (int i = 0; i < ITERATIONS; i++)
  {
#ifdef PRINTING
    struct timeval before, after;
    gettimeofday(&before, NULL);
#endif
    
    for (int j = 0; j < BUFFER_RUNS; j++)
    {
#ifdef __tile__
      do_write(fd, buffer, buf_size);
#else
      do_read(fd, buffer, buf_size);
#endif
    }

#ifdef PRINTING
    gettimeofday(&after, NULL);
    double secs = after.tv_sec - before.tv_sec;
    double usecs = after.tv_usec - before.tv_usec;
    double time = secs + usecs * 1e-6;

    PRINTF("%f seconds for %ld bytes, %f Mb/s\n",
           time, (unsigned long)(buf_size * BUFFER_RUNS), 
           (buf_size * BUFFER_RUNS * 8) / time / 1e6);
#endif
  }
}


int
main(int argc, char** argv)
{
  parse_args(argc, argv);

  int fd = open_channel(chan_idx);

  void* buffer = malloc(buf_size);
  if (buffer == NULL)
  {
    fprintf(stderr, "Failed to allocate buffer.\n");
    exit(EXIT_FAILURE);
  }
  memset(buffer, 0, buf_size);

  test_host_to_tile(fd, buffer, buf_size);
  test_tile_to_host(fd, buffer, buf_size);

  PRINTF("Bandwidth test complete.\n\n");
  close(fd);

  return 0;
}
