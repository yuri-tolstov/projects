Introduction
------------

This example shows how to transfer data between a tile application and a host
application using the Gx PCIe Packet Queue API, with the SR-IOV support.

Using the gxpci library, the tile applications t2h.c and h2t.c
transmits and receives packets to and from the host, respectively.
Each program runs an infinite loop posting buffers and waits for
the data transfer completions. They break out of the loop when
the channel is reset. For tile-to-host transfer, the packets are
written into a ring buffer in host memory. For host-to-tile transfer,
the packets are read from a ring buffer in host memory.

The PQ interfaces are designed to be independent and unidirectional.
Each unidirectional interface is usually managed by a unique process
on the TILE processor, as shown in t2h.c and h2t.c above. On the other hand,
duplex data transfers can be executed by a single TILE process which
spawns separate threads for unidirectional queues. This arrangement is
actually mandatory if 8 pairs of T2H and H2T interfaces are needed.
The tile application duplex.c demonstrates how to implement duplex data
transfers with pthreads.

The host program, host.c, reads a PCIe register to find how much of
the ring buffer has been consumed, to determine how many data buffers
are available for read for tile-to-host and how many data buffers
are empty  for host-to-tile. Reverse flow control is implemented by
the host program writing another PCIe register to report the
available space in the ring buffer. Note that host.c works with both
process-based or thread-based tile side applications as explained above.

It is possible to have different number of tile-to-host and
host-to-tile queues. This can be done by applying identical parameters to
both the TILE Linux kernel and host driver:

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

1. Upload tile_h2t and/or tile_t2h to the target system.
2. Run host_h2t and/or host_t2h on the host.

They will run the host-to-tile tests and tile-to-host tests, respectively,
reporting the total number of packets transferred, packet size, duration,
bits per second and number of packets per second.

For example, to run the tile-to-host test over 4 queues simultaneously,
use the following commands:

1. On the tile side:
   /tile_t2h & /tile_t2h --queue_index=1 --cpu_rank=2 & \
   /tile_t2h --queue_index=2 --cpu_rank=3 & \
   /tile_t2h --queue_index=3 --cpu_rank=4
   The "--cpu_rank" option is used to affinitize the process on the
   specified tile or core. Without it, all the processes will run on the
   same tile and will not achieve the highest performance.

2. On the host side:
   /tmp/host_t2h & /tmp/host_t2h --queue_index=1 & \
   /tmp/host_t2h --queue_index=2 & /tmp/host_t2h --queue_index=3

To run duplex tests in a single TILE process, do something like the following:
1. On the tile side:
   /tile_dup
2. On the host side:
   /tmp/host_t2h & /tmp/host_h2t

To run the tests with host virtual machines, follow the below steps:
1. On the tile side, specify the Gx PCIe Virtual Function number with test
   command opton "--vf". For example, assuming that VF 0 and 1 are attached
   to host VM 0 and 1 respectively, run the following commands to start two
   simultaneous data transfer flows:
   /tile_t2h --vf=0 & /tile_h2t --vf=0 & /tile_t2h --vf=1 & /tile_h2t --vf=1
2. On the host VM0:
   /tmp/host_t2h & /tmp/host_h2t
3. On the host VM1:
   /tmp/host_t2h & /tmp/host_h2t

To run the duplex test with host virtual machines, follow the below steps:
1. On the tile side, specify the Gx PCIe Virtual Function number with test
   command opton "--vf". For example, assuming that VF 15 is attached
   to the host VM 0, run the following commands to start the PCIe traffic:
   /tile_dup --vf=15
2. On the host VM 0, run the following commands:
   /tmp/host_t2h & /tmp/host_h2t

Note:
1. The tile examples only run on Gx chips which are connected to a x86 host.
2. If there are more than one Gx chip connected to a x86 host, select one
to test first by running 'setenv TILERA_DEV gxpciN', where N is the Gx port
index, i.e. '/dev/tilegxpciN'; then update card_index in host.c to N also.
3. If there are more than one Gx endpoint port on a single card such as a
Gx72-based PCIe card, change "/dev/tilegxpci%d/..." to
"/dev/tilegxpci%d-link1/..." in host.c to run the examples on the 2nd port.

Test Caveat
-----------

Due to the fact that the host driver allocates 4MB physically contiguous
memory for the ring buffer, it is possible that this allocation could fail
on a host whose memory has been considerable fragmented. If the host
program exits with the following error, reboot the host and run the test
again.

Host: Failed to open '/dev/tilegxpci%d/packet_queue/t2h/0': Cannot allocate
memory
