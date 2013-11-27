This example shows how to use mPIPE with "fork()".

It initializes mPIPE.  It requests all incoming packets (but drops
jumbo packets).  It forks multiple processes to pull in packets and
forward them.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
