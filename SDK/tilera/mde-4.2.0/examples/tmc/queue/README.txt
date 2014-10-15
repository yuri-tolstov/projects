This program demonstrates how to use a queue/cacheline_queue to
transfer data between two tiles. The program initializes a queue and
binds two threads on two different tiles separately, then starts
single and multiple data sendings on one tile, and data receiving on
the other tile.

User can choose different queue types in the Makefile via EXTRA_FLAGS, by
default the basic queue type is used.

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
