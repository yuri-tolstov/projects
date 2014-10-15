This example shows how to use distribute traffic over one link between
the local Linux and up to three other external interfaces, based on
DMACs.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run:
  app link dmac [link dmac ...]

For each link/dmac pair, packets targeted at that DMAC are forwarded
on to that link.  Packets that don't match any DMAC are broadcast.
The first link is the "uplink", or connection to the outside world.
The first dmac is forwarded to the local Linux stack.
