This example shows how to connect two simulator instances via
tile-monitor.  Using the "peered simulator" commands, each simulator's
TRIO shim is configured to connect to the other simulator's TRIO
shim.

This particular example uses peered simulators to demonstrate TRIO's
MapMem, PIO, and push DMA capabilities.  Each peer runs the same code,
performing several DMA operations, checking results, and using PIO to
perform barriers between peers.

See the Application Libraries Reference Guide for more information
about the gxio APIs, and IO Device Guide for more information about
TRIO.  See the tile-monitor and tile-sim man pages for more
information about simulator peering.

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make test_sim
