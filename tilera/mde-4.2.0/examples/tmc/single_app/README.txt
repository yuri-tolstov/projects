This example program demonstrates a single-app parallel execution style,
in which an application fork/execs multiple instances of itself,
affinitized one per tile, and the instances then work together to execute
the application, using the UDN for communication between instances.

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
