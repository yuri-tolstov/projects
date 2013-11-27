Introduction
------------

This example shows how to use the Gx PCIe Packet Queue API in combination with
the Linux TAP driver to implement a packet path between host kernel and tile
kernel.  The test scenario is as shown below:

                                            |
                                       Tile | Host
                                            |
  +-------------+                           |              +-------------+
  |   Socket    |                           |              |   Socket    |
  | Application |                           |              | Application |
  +-------------+                           |              +-------------+
            ^                               |                       ^    
            |      +----------+             |         +----------+  |    
            |      |          |   Packet Queue I/F    |          |  |    
            |      | tile_tap |<--------------------->| host_tap |  |    
            |      |          |             |         |          |  |    
            |      +----------+             |         +----------+  |    
            |               ^               |           ^           |    
            |               |               |           |           |    
 User       |               |               |           |           |    
------------|---------------|---------------------------|-----------|-------
 Kernel     v               | /dev/net/tun  |           |           v
      +-------------------+ |               |           | +----------------+
      |                   | |               |           | |                |
      |  Linux IP Stack   | |               |           | | Linux IP Stack |
      |                   | |               |           | |                |
      +-------------------+ |               |           | +----------------+
                   ^        |               |           |    ^       
        netdev API |        |               |           |    |       
                   v        v               |           v    v       
                +-----------------+         |       +-----------------+
                |     TUN/TAP     |         |       |     TUN/TAP     |
                |      Driver     |         |       |      Driver     |
                +-----------------+         |       +-----------------+
 

1. TILE Linux kernel: adding '--hvx pcie_host_pq_h2t_queues=T,P,N' and
'--hvx pcie_host_pq_t2h_queues=T,P,M' when booting the Gx chip, where T is
the TRIO instance number, P is the port number, N is the host-to-tile queue
number and M is the tile-to-host queue number.

2. Host driver: adding 'pq_h2t_queues=N' and 'pq_t2h_queues=M' when inserting
the host driver, where N is the host-to-tile queue number and M is the
tile-to-host queue number. 

Note that the sum of tile-to-host and host-to-tile queues can not be larger
than 16.

It is also possible to specify the host ring buffer size and attributes that
are different from the default settings. Please refer to the document
Host PCIe Packet Queue API, AN044, for details.

Running The Example
-------------------

To run the example, set the TILERA_ROOT environment variable and then do:

1. Upload tile_tap to the target system and run it
2. Run host_tap (as root) on the host
3. Verify connectivity through the Linux stack, using ping or other traffic
   generation tools

tile_tap options:
-----------------
  --mac=N        - PCIe MAC to use
  --cpu-rank=N   - CPU affinity control.  The TAP application uses two CPUs
                   (N and N + 1)
  --queue=N      - PQ queue pair index to use for communication with the host
  --if-name=xxx  - Name to configure the TUN device with.  This defaults to
                   gxpci<N>-<M>, where N = MAC number, and M = queue index
  --if-addr=<IP> - IPv4 Address to configure the TUN device with.  This is
                   optional.  The application leaves the address unconfigured
                   if this parameter is not specified.
  --no-restart   - The application's default behavior is to reinitialize
                   itself on PQ link failure.  This parameter forces the
                   application to exit on failure instead.
  --vf		 - The SR-IOV virtual function number.

host_tap options:
-----------------
  --card=N       - GXPCI card to use
  --queue=N      - <same as for tile_tap>
  --if-name=xxx  - <same as for tile_tap>
  --if-addr=<IP> - <same as for tile_tap>
  --no-restart   - <same as for tile_tap>
