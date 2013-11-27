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
// Host-side part of USB device example.
//

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <linux/usbdevice_fs.h>

#include "ud_intf.h"
#include "util.h"

/** Name of this program, for error messages. */
const char* myname;

//
// Command options, first the long versions.
//
static const struct option long_options[] =
{
  { .name = "alt",                .has_arg = 0, .val = 'a' },
  { .name = "compare",            .has_arg = 0, .val = 'c' },
  { .name = "dev",                .has_arg = 1, .val = 'D' },
  { .name = "download",           .has_arg = 0, .val = 'd' },
  { .name = "file",               .has_arg = 1, .val = 'f' },
  { .name = "help",               .has_arg = 0, .val = 'h' },
  { .name = "input",              .has_arg = 1, .val = 'i' },
  { .name = "loops",              .has_arg = 1, .val = 'l' },
  { .name = "number",             .has_arg = 0, .val = 'n' },
  { .name = "output",             .has_arg = 1, .val = 'o' },
  { .name = "size",               .has_arg = 1, .val = 's' },
  { .name = "upload",             .has_arg = 0, .val = 'u' },
  { 0 },
};

//
// Now the short versions.
//
static const char options[] = "acdD:f:hi:l:no:u";


//
// Flags set by option parsing to tell us if various options have been seen.
//
static int opt_a = 0;        // --alt
static int opt_c = 0;        // --compare
static int opt_d = 0;        // --download
static char* opt_D = NULL;   // --dev
static int opt_f = -1;       // --file
static int opt_l = 1;        // --loops
static int opt_n = 0;        // --number
static long opt_s = 0;       // --size
static int opt_u = 0;        // --upload

#define MAX_FILES 10          // Maximum number of input or output files.
static int n_ins = 0;         // Number of input files
static int stdins_seen;       // Number of input files which are "-"
static char* ins[MAX_FILES];  // Input file names
static int n_outs = 0;        // Number of output files
static int stdouts_seen;      // Number of output files which are "-"
static char* outs[MAX_FILES]; // Output file names


/** Print a usage message and exit.
 * @param exstat Exit status.
 */
void
usage(int exstat)
{
  fprintf(stderr, "\
usage: ux { --upload | --download | --compare } <options>\n\
  --upload                Transfer a file to the USB device.\n\
  --download              Transfer a file from the USB device.\n\
  --compare               Transfer a file to the USB device; transfer the\n\
                          same file back to the host; compare the two files.\n\
\n\
  --alt                   Use alternate set of USB interfaces/endpoints.\n\
  --dev <bus>.<dev>       Specify USB device to use.  If omitted, and only\n\
                          one suitable device exists, it will be used.\n\
  --file <n>              Use file index <n> on the USB device.  Default 1,\n\
                          or 2 if --alt is specified.\n\
  --help                  Print this message.\n\
  --input <file>          Specify input file for --upload; if not specified,\n\
                          zeroes are used.  - may be specified to use stdin.\n\
                          Multiple files may be specified, and if so are used\n\
                          in turn on successive loops.\n\
  --loops <n>             Perform upload, download, or compare operation <n>\n\
                          times; 0 means to repeat forever.  Default 1.\n\
  --number                Replace the first 8 bytes of each 512 bytes in\n\
                          an uploaded file with an increasing packet number;\n\
                          when -n enabled, --compare checks only these bytes.\n\
                          Best when uploading zeroes; not likely to work when\n\
                          input is a pipe-like device.\n\
  --output <file>         Specify output file for --download; if not\n\
                          specified, data is discarded.  - may be specified\n\
                          to use stdout.  Multiple files may be specified,\n\
                          and if so are used in turn on successive loops.\n\
  --size <size>           Transfer <size> bytes; only useful when an input or\n\
                          output file is omitted.\n\
"
  );

  exit(exstat);
}


/** Search the list of USB devices for something that matches our
 *  vendor/dev IDs.  If we don't find exactly one such device, print an
 *  error message and exit.
 * @param bus Pointer to returned bus number.
 * @param dev Pointer to retured device address.
 */
void
find_usb_dev(int* bus, int* dev)
{
  FILE* devs = fopen("/proc/bus/usb/devices", "r");
  if (!devs)
    pbomb("can't open /proc/bus/usb/devices");

  char line[128]; 

  int need_t_line = 1;
  int devs_found = 0;
  int last_bus = 0;
  int last_dev = 0;

  while (fgets(line, sizeof (line), devs))
  {
    //
    // Somewhat annoyingly, the T: line that tells you the address comes
    // before the P: line that tells you the product/vendor ID, so we have
    // to look for the former and save it until we see the latter.
    //
    if (need_t_line)
    {
      if (line[0] != 'T')
        continue;

      if (sscanf(line, "T: Bus=%d Lev=%*d Prnt=%*d Port=%*d Cnt=%*d Dev#=%d ",
                 &last_bus, &last_dev) != 2)
        continue;

      need_t_line = 0;
    }
    else
    {
      if (line[0] != 'P')
        continue;

      unsigned int vendor;
      unsigned int product;

      if (sscanf(line, "P: Vendor=%x ProdID=%x ", &vendor, &product) == 2 &&
          vendor == UD_DEV_VENDOR && product == UD_DEV_PRODUCT)
      {
        devs_found++;
        if (devs_found == 2)
        {
          fprintf(stderr, "%s: more than one suitable device exists, "
                  "must use --dev option.  Devices:\n", myname);
          fprintf(stderr, "    %d.%d\n", *bus, *dev);
        }

        if (devs_found >= 2)
          fprintf(stderr, "    %d.%d\n", last_bus, last_dev);

        *bus = last_bus;
        *dev = last_dev;
      }

      need_t_line = 1;
    }
  }

  fclose(devs);

  if (devs_found <= 0)
    fprintf(stderr, "%s: no suitable USB devices found\n", myname);

  if (devs_found != 1)
    exit(1);
}

/** Input sequence number. */
static unsigned long in_seq;
/** Output sequence number. */
static unsigned long out_seq;
/** Packet size; sequence numbers increase once per packet. */
static const int pkt_size = 512;

/** USB filesystem transfer size.  On most kernels, usbfs only supports 16 kB
 *  buffers. */
static const size_t usbfs_xfer_size = 16 * 1024;

/** Data buffer; usbfs_xfer_size bytes. */
static char* data;

/** Same buffer as data, just a different type for convenience. */
static uint64_t* data64;

/** Print summary performance statistics.
 * @param s_time Start time for transfer.
 * @param e_time End time for transfer.
 * @param xfer_size Bytes transferred.
 * @param op String describing transfer (e.g., "read", "write").
 */
void
print_summary(struct timeval* s_time, struct timeval* e_time, long xfer_size,
              char* op)
{
  double elapsed =
    e_time->tv_sec - s_time->tv_sec +
    (double) (e_time->tv_usec - s_time->tv_usec) / 1000000.0;

  fprintf(stderr, "%ld bytes %-7s in %5.2f seconds; %5.2f Mbytes/s, "
          "%6.2f Mbits/s\n", xfer_size, op, elapsed,
          xfer_size / (1000000 * elapsed),
          8 * xfer_size / (1000000 * elapsed));
}


/** Issue the control request to the device which sets the input or output
 *  file index for a specific endpoint.
 * @param usb_fd USB filesystem file descriptor.
 * @param file_idx File index.
 * @param ep Endpoint.
 */
void
set_file_ctrl(int usb_fd, int file_idx, int ep)
{
  static unsigned char ctrl_dummy[1];

  //
  // The control operation reads a byte from the device.  We don't actually
  // use it, but the fact that we do the read ensures that we don't issue
  // any subsequent IN or OUT operations until the device has performed the
  // control op.
  //
  static struct usbdevfs_ctrltransfer ctrl =
  {
    .bRequestType = 0xC2,
    .bRequest = UD_SET_FILE,
    .wLength = 1,
    .timeout = 1000,
    .data = ctrl_dummy,
  };

  ctrl.wIndex = ep & 0x7F;
  ctrl.wValue = file_idx;
  ctrl_dummy[0] = 0;

  int stat = ioctl(usb_fd, USBDEVFS_CONTROL, &ctrl);
  if (stat != 1)
    pbomb("set-file control failure");
}


/** Upload a file.
 * @param usb_fd USB filesystem file descriptor.
 * @param inf Input file name; may be "-" for stdin, or NULL to generate
 *  zeroes.
 */
void
upload(int usb_fd, char* inf)
{
  int fd;
  
  if (inf == NULL)
  {
    //
    // We'll use a negative fd below to skip the read-from-file step.
    //
    fd = -1;
    memset(data, 0, usbfs_xfer_size);
  }
  else if (!strcmp(inf, "-"))
  {
    fd = dup(fileno(stdin));
  }
  else
  {
    fd = open(inf, O_RDONLY, 0);
    if (fd < 0)
      pbomb("couldn't open input %s", inf);
  }

  struct usbdevfs_bulktransfer bulk =
  {
    .data = data,
    .timeout = 1000,
    .ep = (opt_a ? 2 : 0) + 2,
  };

  set_file_ctrl(usb_fd, opt_f, bulk.ep);

  //
  // In the normal case, where we aren't limiting the number of bytes in
  // the uploaded file via --size, we assume that the file has fewer than
  // UINT64_MAX bytes; a pretty safe assumption.
  //
  uint64_t bytesleft = opt_s ? opt_s : UINT64_MAX;
  long byteswritten = 0;

  struct timeval s_time;
  gettimeofday(&s_time, NULL);

  while (bytesleft)
  {
    int len = min(bytesleft, usbfs_xfer_size);

    if (fd >= 0)
    {
      len = read(fd, data, len);
      if (len < 0)
        pbomb("can't read input %s", inf);

      if (len == 0)
        break;
    }

    bulk.len = len;

    if (opt_n)
    {
      //
      // Overwrite the first 8 bytes of every packet with the output
      // sequence number.
      //
      for (int i = 0; i < (bulk.len + pkt_size - sizeof (*data64)) / pkt_size;
           i++)
        data64[i * (pkt_size / sizeof (*data64))] = out_seq + i;
    }

    int data_len = ioctl(usb_fd, USBDEVFS_BULK, &bulk);

    if (data_len != bulk.len)
    {
      if (data_len < 0)
        pbomb("error on bulk USB write");
      else
        bomb("short bulk USB write @ pos %ld, %d != %d, "
             "in_seq 0x%lx out_seq 0x%lx",
             byteswritten, data_len, bulk.len, in_seq, out_seq);
    }

    out_seq += data_len / pkt_size;
    bytesleft -= data_len;
    byteswritten += data_len;
  }

  struct timeval e_time;
  gettimeofday(&e_time, NULL);

  print_summary(&s_time, &e_time, byteswritten, "written");

  close(fd);
}


/** Download a file, and do sequence number checking if --compare and --number
 *  were specified.
 * @param usb_fd USB filesystem file descriptor.
 * @param outf Output file name; may be "-" for stdout, or NULL to throw
 *  away output data.
 */
void
download(int usb_fd, char* outf)
{
  int fd;
  
  if (outf == NULL)
  {
    //
    // We'll use a negative fd below to skip the write-to-file step.
    //
    fd = -1;
  }
  else if (!strcmp(outf, "-"))
  {
    fd = dup(fileno(stdout));
  }
  else
  {
    fd = open(outf, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 1)
      pbomb("couldn't open output %s", outf);
  }

  struct usbdevfs_bulktransfer bulk =
  {
    .data = data,
    .timeout = 1000,
    .ep = (opt_a ? 2 : 0) + 0x81,
  };

  set_file_ctrl(usb_fd, opt_f, bulk.ep);

  //
  // In the normal case, where we aren't limiting the number of bytes in
  // the downloaded file via --size, we assume that the file has fewer than
  // UINT64_MAX bytes; a pretty safe assumption.
  //
  uint64_t bytesleft = opt_s ? opt_s : UINT64_MAX;
  long bytesread = 0;

  struct timeval s_time;
  gettimeofday(&s_time, NULL);

  while (bytesleft)
  {
    bulk.len = min(bytesleft, usbfs_xfer_size);

    int data_len = ioctl(usb_fd, USBDEVFS_BULK, &bulk);

    if (data_len < 0)
      pbomb("error on bulk USB read");

    if (data_len == 0)
      break;

    if (fd >= 0 && write(fd, data, data_len) != data_len)
      pbomb("short file write");

    if (opt_n && opt_c)
    {
      int bad_compare = 0;

      //
      // Compare sequence numbers here.  Note that we intentionally
      // don't exit on the first error; we want to see if it's just
      // one dropped packet, or worse.  There are only 32 packets
      // per buffer so it's not too much extra output.
      //
      for (int i = 0; i < (data_len + pkt_size - sizeof (*data64)) / pkt_size;
           i++)
      {
        long expected_seq = in_seq + i;
        long actual_seq = data64[i * (pkt_size / sizeof (*data64))];
        if (actual_seq != expected_seq)
        {
          fprintf(stderr, "%s: input buffer 0x%lx instead "
                  "labeled as buffer 0x%lx\n", myname, expected_seq,
                  actual_seq);
          bad_compare = 1;
        }
      }

      if (bad_compare)
        exit(1);
    }

    in_seq += data_len / pkt_size;

    bytesleft -= data_len;
    bytesread += data_len;
  }

  struct timeval e_time;
  gettimeofday(&e_time, NULL);

  print_summary(&s_time, &e_time, bytesread, "read");

  close(fd);
}

/** Compare an input and output file.  If the comparison fails, print an
 *  error message and exit; if it succeeds, delete the output file.
 * @param inf Name of input file.
 * @param outf Name of output file.
 */
void
compare(char* inf, char* outf)
{
  char* cmdfmt = "cmp %s %s";
  char cmd[strlen(cmdfmt) + strlen(inf) + strlen(outf)];
  sprintf(cmd, cmdfmt, inf, outf);
  int status = system(cmd);
  if (status < 0)
    bomb("couldn't do file comparison");

  if (WEXITSTATUS(status) != 0)
    bomb("file comparison error");

  unlink(outf);
}


/** Main program. */
int
main(int argc, char** argv)
{
  myname = "ux";

  //
  // Parse our arguments.
  //
  int opt;

  while ((opt = getopt_long(argc, argv, options, long_options, NULL)) > 0)
  {
    switch (opt)
    {
    case 'a':   // --alt
      opt_a = 1;
      break;

    case 'c':   // --compare
      opt_c = 1;
      break;

    case 'd':   // --download
      opt_d = 1;
      break;

    case 'D':   // --dev
      opt_D = optarg;
      break;

    case 'f':   // --file
      opt_f = strtol(optarg, NULL, 0) & 0xFFFF;
      break;

    case 'h':  // --help
      usage(0);
      break;

    case 'i':   // --input
      if (n_ins >= MAX_FILES)
        bomb("can't have more than %d input files", MAX_FILES);
      ins[n_ins++] = optarg;
      stdins_seen += !strcmp(optarg, "-");
      break;

    case 'l':   // --loops
      opt_l = strtol(optarg, NULL, 0);
      break;

    case 'n':   // --number
      opt_n = 1;
      break;

    case 'o':   // --output
      if (n_outs >= MAX_FILES)
        bomb("can't have more than %d output files", MAX_FILES);
      outs[n_outs++] = optarg;
      stdouts_seen += !strcmp(optarg, "-");
      break;

    case 's':   // --size
      opt_s = strtol_with_suffix(optarg);
      break;

    case 'u':   // --upload
      opt_u = 1;
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
  // Validate combinations of parameters and set defaults.
  //
  if (opt_c)
    opt_d = opt_u = 1;

  if (!opt_d && !opt_u)
    bomb("must specify --upload, --download, or --compare");

  if (!opt_u && n_ins > 0)
    bomb("must specify --upload or --compare with --input");

  if (!opt_d && n_outs > 0)
    bomb("must specify --download or --compare with --output");

  if (opt_u && n_ins <= 0 && opt_s == 0)
    bomb("must specify --size when uploading without an input file");

  if (opt_l != 0 && (n_ins > opt_l || n_outs > opt_l))
    bomb("can't specify more input or output files than loops");

  if (stdins_seen > 1 || stdouts_seen > 1)
    bomb("can't specify stdin/stdout more than once");

  if ((opt_c && !opt_n) && (stdins_seen > 0 || stdouts_seen > 0))
    bomb("can't compare stdin/stdout unless -n specified");

  if ((opt_c && !opt_n) && (n_ins <= 0 || n_outs <= 0))
    bomb("can't compare without actual input/output files "
         "unless -n specified");

  if (opt_f < 0)
    opt_f = opt_a ? 2 : 1;

  if (n_ins == 0)
  {
    n_ins = 1;
    ins[0] = NULL;
  }

  if (n_outs == 0)
  {
    n_outs = 1;
    outs[0] = NULL;
  }

  //
  // Open the USB device.
  //
  int bus = 0;
  int dev = 0;

  if (opt_D)
  {
    if (sscanf(opt_D, "%d.%d", &bus, &dev) != 2)
      bomb("bad --dev format, should be <bus>.<dev>");
  }
  else
  {
    find_usb_dev(&bus, &dev);
  }

  char dev_str[80];
  sprintf(dev_str, "/proc/bus/usb/%03d/%03d", bus, dev);

  int usb_fd = open(dev_str, O_RDWR);
  if (usb_fd < 0)
    pbomb("can't open USB device");

  //
  // Claim our interfaces.
  //
  int in_intf = opt_a ? 2 : 0;
  if (ioctl(usb_fd, USBDEVFS_CLAIMINTERFACE, &in_intf) < 0)
    pbomb("couldn't claim input interface %d", in_intf);

  int out_intf = opt_a ? 3 : 1;
  if (ioctl(usb_fd, USBDEVFS_CLAIMINTERFACE, &out_intf) < 0)
    pbomb("couldn't claim output interface %d", out_intf);

  data = malloc(usbfs_xfer_size);
  if (!data)
    bomb("cannot allocate memory");

  data64 = (uint64_t*) data;

  int in_idx = 0;
  int out_idx = 0;
  in_seq = opt_a ? 0x8000000000000000UL : 0;
  out_seq = opt_a ? 0x8000000000000000UL : 0;

  for (int i = 0; opt_l <= 0 || i < opt_l; i++)
  {
    char* inf = ins[in_idx];
    in_idx = (in_idx + 1) % n_ins;
    char* outf = outs[out_idx];
    out_idx = (out_idx + 1) % n_outs;

    if (opt_u)
      upload(usb_fd, inf);

    if (opt_d)
      download(usb_fd, outf);

    if (opt_c && !opt_n)
      compare(inf, outf);
  }

  free(data);

  //
  // Release our interfaces, close the USB device.
  //
  if (ioctl(usb_fd, USBDEVFS_RELEASEINTERFACE, &in_intf) < 0)
    pbomb("couldn't release input interface %d", in_intf);

  if (ioctl(usb_fd, USBDEVFS_RELEASEINTERFACE, &out_intf) < 0)
    pbomb("couldn't release output interface %d", out_intf);

  close(usb_fd);
}
