This example shows how to establish a pair of data transfer queues
that move data packets between two applications running on two Gx nodes.
A sender thread and a receiver thread are created by the program c2c.c,
talking to the receiver thread and the sender thread that are created
by the same c2c.c program running on another Gx node, respectively.

The example program takes the following parameters:
	--mac=<local mac port #>
		the PCIe MAC number on local Gx, with default value 0.
	--dont_recv=<0 or 1>
		a value of 1 disables the receiver thread.
	--dont_send=<0 or 1>
		a value of 1 disables the sender thread.
	--rem_link_index=<remote port link index>
		the PCI domain-wide link index of the remote PCIe port,
		with default value 1.
	--send_queue_index=<send queue index>
		the index of the send queue, in [0, 63] with default value 1,
		which must be identical to the remote receive queue's index.
	--recv_queue_index=<receive queue index>
		the index of the receive queue, in [0, 63] with default value 2,
		which must be identical to the remote send queue's index.
	--send_pkt_size=<packet size>
		the size of each packet. This parameter is only used for
		calculating the performance result and is not dependent on in
		the data transfer process. The value must be the same
		on both sides.
	--send_cpu_rank=<core #>
		ID of the core running the send thread, 1 by default.
	--recv_cpu_rank=<core #>
		ID of the core running the receive thread, 2 by default.
	--send_pkt_count=<packet count>
		the total number of packets that are transferred by the sender.
		The parameter value must be identical on both sides.

To run the example:
	1. make clean;make
	2. On one Gx with local link index Y:
	   /c2c --rem_link_index=X --send_queue_index=M --recv_queue_index=N
	3. On another Gx with local link index X:
	   /c2c --rem_link_index=Y --send_queue_index=N --recv_queue_index=M

How to determine the local PCIe port's link index:
	1. On a root-complex port, its link index is always 0.
	2. On a endpoint port, it is the number in the last column of file
	   /proc/driver/gxpci_ep_major_mac_link.

