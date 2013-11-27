This example shows how to connect two simulator instances via
tile-monitor.  Using the "peered simulator" commands, each simulator's
mPIPE shim is configured to connect to the other simulator's mPIPE
shim.  This capability is particularly useful for developing
applications that require dynamic packet generation rather than just
static pcap files.

This particular example uses peered simulators to bring up the Linux
networking stack and run 'ping'.  More complicated scenarios can be
built using peered simulators; for example one simulator could run a
web server and the other could run a http request generator.

See the tile-monitor and tile-sim man pages for more information about
simulator peering.

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make test_sim
