Introduction
------------

This example demonstrates the use of the TILE-Gx USB Device interface.
The USB device interface allows the TILE-Gx processor to play the role
of a USB device which communicates with a USB host.  There are two parts
to the example:

- A tile-side program, ud, plays the role of the USB device.  It implements
  very simple interfaces which support uploading data to, and downloading
  data from, the TILE-Gx processor, via the USB.

- A host-side program, ux, drives the device.  It allows you to upload
  files to and download files from the device.  It also has an option
  which compares the uploaded and downloaded files, as well as a looping
  option, both of which may be useful for hardware stress testing.


Preparing The Host System
-------------------------

In order to gain access to the USB, the host-side part of the example, ux,
uses the Linux USB filesystem interfaces.  By default, the special files
which provide access to the USB filesystem are protected from access by
non-root users.  Thus, to run ux, you will need to do one of two things:

1. Run it as root.

2. Modify the system startup scripts to change the default permissions on
   the relevant special files, and reboot the system.  To do this on Red
   Hat Linux, edit /etc/rc.sysinit, and in two places, change the mount
   command for usbfs from this:

      mount -n -t usbfs /proc/bus/usb /proc/bus/usb

   to this:

      mount -n -t usbfs -o devmode=0666 /proc/bus/usb /proc/bus/usb

   Note that making this change may have security implications, as it gives
   user processes unrestricted access to USB devices.  It's probably fine
   to do on a machine you are using for development and testing, but not
   on a machine used for production.  If this is a concern, but you don't
   want to run ux as root, note that it is possible to set things up so
   that only certain users gain this access, using the devuid or devgid
   options; consult your distribution's documentation for details.


Preparing the Target System
---------------------------

Obviously, in order to communicate with the system which will be playing
the role of the device, you will need a USB connection between it and the
host system.  Perhaps less obviously, since you will be configuring the
USB interface on the target in application device mode, you will not be
able to subsequently use that interface with tile-monitor or tile-btk.
Thus, if you are using USB to initially boot the target, you will need
to have another way to communicate with the system, as well as a way to
reboot the system once you are done testing.

To communicate with the system, you might use either a direct connection
to the serial console, or a network connection.  To reboot the system, you
could use a PCIe connection (for instance, if you are using a TILEncore
card); you could reboot the system from SROM; or you could power-cycle
the machine, which would reenable boot/debug access via USB.


Building The Example
--------------------

To build the example, set the TILERA_ROOT environment variable to the base
directory of your Tilera MDE installation.  You may then do:

  make

to compile and link both the host-side and device-side programs.


Running The Example
-------------------

To run the example, you must first copy the ud program to the TILE-Gx
system which will serve as the device.  You may run

  make upload

to boot the default TILE-Gx processor and upload the device-side program
to it (as /opt/test/ud), you may run tile-monitor by hand to do that, or
you may get it there via some other means (e.g., using ssh).  Note that
ud requires access to the gxio library on the target system.  Also note
that the system's hypervisor configuration file must be configured to
enable use of the device shim; the easiest way to do this is to specify
--hvd USB_DEV when running tile-monitor or tile-mkboot.

Once the target is booted, you can run ud on the target to start application
device mode operation, as follows.  Do not try to do this via tile-monitor
over USB, since as soon as ud starts the tile-monitor connection would
be disrupted.

  # /opt/test/ud

Once the device is running, you may try using ux to upload and download
some data, for example:

  $ ux --compare --number --size 128m
  134217728 bytes written in  5.13 seconds; 26.16 Mbytes/s, 209.28 Mbits/s
  134217728 bytes read    in  5.12 seconds; 26.20 Mbytes/s, 209.63 Mbits/s

For a more thorough test, you might want to generate some random data:

  $ dd if=/dev/urandom of=f1a bs=1024k count=128
  $ dd if=/dev/urandom of=f1b bs=1024k count=128
  $ dd if=/dev/urandom of=f2a bs=1024k count=121
  $ dd if=/dev/urandom of=f2b bs=1024k count=121

and then run two copies of ux simultaneously:

  $ ux -i f1a -o f1a_in -i f1b -o f1b_in -c -l 100

  (and in another window)

  $ ux -a -i f2a -o f2a_in -i f2b -o f2b_in -c -l 100

When done, you should kill the ud process and reboot the target.

Caveats
-------

The USB device implemented by ud has pretty much the bare minimum behavior
required to get it to work with a Red Hat Linux host system running the
ux example program.  There are a number of ways in which it is not a
complete device implementation, notably:

- ud is not fully compliant with the USB specification; in particular, it
  does not implement the required behavior in many error cases.  If you
  build an application starting with this example, expect to have to do
  significant work in order to end up with a fully compliant USB device.

- ud has not been tested with, and thus may well not work with, other
  USB hosts which have different device discovery behavior than RHEL.

- ud does not support USB 1.1.

- ud does not properly handle exception cases like link suspend or link
  disconnect and reconnect.

Production applications may be better off using the Linux USB Gadget
interface, which is planned for delivery in a future version of the MDE,
and which should eliminate at least some of these limitations.


ud documentation
----------------

The only option available for ud is the --debug option, which will
cause it to print messages when it gets or handles a USB transaction.
Note that performance will be substantially reduced when this option is
used, potentially so much so that USB operations time out and fail.


ux documentation
----------------

When invoking ux, exactly one of the following three options must be
specified:

  -u | --upload
    Transfer a file to the USB device.

  -d | --download
    Transfer a file from the USB device.

  -c | --compare
    Transfer a file to the USB device; transfer the same file back to the
    host; compare the two files; exit if any differences are found.

You may also specify one or more of the following:

  -a | --alt
    Use an alternate set of USB interfaces/endpoints.  In order to run
    two instances of ux simultaneously, doing the same operation (e.g.,
    both uploading), this option must be specified on exactly one of the
    two instances.

  -D --dev <bus#>.<device#>
    Specify USB device to use.  Typically, this option is omitted; in
    that case, if only one suitable device exists, it will be used.

  -f | --file <n>
    Use file index <n> on the USB device.  A file index is a small integer
    used to tag an uploaded file, so that it can be uniquely retrieved
    afterward; this is important when running multiple simultaneous copies
    of ux.  The default is 1, or 2 if --alt is specified.

  -i | --input <file>
    Specify an input file for --upload; if not specified, an infinite
    stream of zero bytes is used.  A file name of - may be specified to
    use stdin.  Multiple input files may be specified, and if so are used
    in turn on successive loops.

  -l | --loops <n>
    Perform the upload, download, or compare operation <n> times; 0 means
    to repeat forever.  Default 1.

  -n | --number
    Replace the first 8 bytes of each 512 bytes in an uploaded file with
    an increasing packet number; when -n enabled, --compare checks only
    these bytes.  This works best when uploading zeroes, and is not likely
    to work the way you expect when the input file is a pipe-like device.

  -o | --output <file>
    Specify an output file for --download; if not specified, the downloaded
    data is discarded.  A file name of - may be specified to use stdout.
    Multiple output files may be specified, and if so are used in turn
    on successive loops.

  -s | --size <size>
    Stop after <size> bytes.  Required with --upload or --compare when
    there is no input file.  Also used when the input file is an infinite
    stream (like /dev/zero), and almost essential in that case, since
    without it, ux will upload an infinite amount of data to the device,
    eventually filling up its memory or disk.
