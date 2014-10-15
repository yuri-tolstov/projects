This example shows how to use chained buffers in an mPIPE app.

It initializes mPIPE.  It requests packets on a loopback channel
(but drops jumbo packets).  It seeds the loopback channel with
some packets.  It creates multiple threads to forward incoming
packets.

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
