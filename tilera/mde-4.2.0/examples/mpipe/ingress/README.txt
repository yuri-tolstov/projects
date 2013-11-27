This example shows how to do simple packet ingress with mPIPE.

It initializes mPIPE (for ingress only).  It receives incoming
packets, and analyzes them.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
