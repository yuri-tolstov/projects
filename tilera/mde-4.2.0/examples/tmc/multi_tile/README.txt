This program demonstrates how to start a multi-process application,
binding each task to a different CPU and initializing the UDN. The
program computes the sum of an array of integers in shared memory by
having each cpu compute the sum of a portion of the array, and then
using the UDN to compute the sum-of-sums.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run on hardware:
  TILERA_ROOT=<path_to_MDE> make run_dev

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim

To run natively:
  make run
