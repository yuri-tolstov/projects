The example shows how to access the mPIPE timestamp registers.

1. testptp.c shows how to access mPIPE timestamp registers via standard
   system call, i.e. clock_gettime, clock_settime and clock_adjtime.
   testptp.c can also be found in the upstream Linux source: Documentation/ptp/

   Because PTP clock device driver depends on the mPIPE driver initialization
   function, if one or more native network interfaces are managed by
   mPIPE driver/Linux kernel, this method can be used.

2. The app.c gives an example to access the mPIPE timestamp registers through
   user space gxio libraries.

   This method can be used when either native network interfaces are managed by
   Linux kernel or by the user space application (i.e. --hvx TLR_NETWORK=none).

To build:
  TILERA_ROOT=<path_to_MDE> make

Example usage:
  ./testptp -h
  ./app -h
