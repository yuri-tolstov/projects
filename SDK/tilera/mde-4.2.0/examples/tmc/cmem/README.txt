The following program demonstrates the use of Common Memory, memory
that is shared between multiple processes, mapped at the same address
in all processes, and can dynamically allocated and freed.

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
