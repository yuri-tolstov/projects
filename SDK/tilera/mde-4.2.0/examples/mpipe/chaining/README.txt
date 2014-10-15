This example shows how to use chained buffers with mPIPE.

It initializes mPIPE.  It requests packets on a loopback channel,
using small buffers with chaining.  It walks through the bytes in
chained buffers.  It forwards chained buffers transparently.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run on hardware:
  TILERA_ROOT=<path_to_MDE> make run_dev

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim

To run natively (assuming "--hvx dataplane=1-999"):
  make run
