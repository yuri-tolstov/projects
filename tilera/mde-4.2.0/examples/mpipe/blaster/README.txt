This example shows how to egress packets loaded from a pcap file.

It initializes mPIPE.  It creates a equeue and a buffer stack.  It
loads the requested pcap file into memory, and registers the pages
used for those packets with the buffer stack.  It blasts the packets
(at line rate) on the requested link.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
