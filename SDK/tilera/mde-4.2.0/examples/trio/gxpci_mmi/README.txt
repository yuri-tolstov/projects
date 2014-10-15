This example shows how to establish memory map region's interrupts (mmi)
in user-space along with tmc/interrupt lib. Both TILE side MMIO access and 
PICe packets which fall into MAP_MEM_INTx region to trigger different MAP_MEM 
interrupts are implemented, which is selected via a macro named
"PCIE_PKT_TRIGGER".

See the Application Libraries Reference Guide for more information about
the gxpci APIs, gxio APIs, and IO Device Guide for more information about
TRIO.  See the tile-monitor and tile-sim man pages for more
information about simulator peering.

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make test_sim
