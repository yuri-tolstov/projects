This directory includes two examples that run in different environments:

1. c2c_send.c
	This example shows how to establish a chip-to-chip data transfer queue
	for PCIe data transmission using two simulator instances via
	tile-monitor.  Using the "peered simulator" commands, each simulator's
	TRIO shim is configured to connect to the other simulator's TRIO shim.

	This particular example uses peered simulators to demonstrate how to 
	write Gx chip-to-chip applications using the gxpci library which
	provides the high-level API to the low-level TRIO gxio library.

	See the Application Libraries Reference Guide for more information about
	the gxpci APIs, gxio APIs, and IO Device Guide for more information
	about TRIO.  See the tile-monitor and tile-sim man pages for more
	information about simulator peering.

	To run under the simulator:
	  TILERA_ROOT=<path_to_MDE> make test_sim

2. c2c_sender.c, c2c_receiver.c
	This example shows how to establish a chip-to-chip data transfer queue
	for PCIe data transmission using two programs running on real HW.
	The c2c_sender.c program, running on one Gx chip, sends data blocks to
	the c2c_receiver.c program running on another Gx chip. Both programs
	take the following parameters:
	--mac=<local mac port #>
		the PCIe MAC number on local Gx, with default value 0.
	--rem_link_index=<remote port link index>
		the PCI domain-wide link index of the remote PCIe port,
		with default value 0.
	--queue_index=<send queue index>
		the index of the chip-to-chip queue, which must be identical
		for the sender and the receiver programs, with default value 0.
	--send_pkt_size=<packet size>
		the size of each packet. This parameter is only used for
		calculating the performance result and is not dependent on in
		the data transfer process. The parameter values must be
		identical for both c2c_sender and c2c_receiver.
	--send_pkt_count=<packet count>
		the total number of packets that are transferred by the sender.
		The parameter values must be identical for both c2c_sender
		and c2c_receiver.

	To run the example:
	1. make clean;make
	2. On one Gx: /c2c_sender --rem_link_index=X --queue_index=N
	3. On another Gx: /c2c_receiver --rem_link_index=Y --queue_index=N

	How to determine the local PCIe port's link index:
	1. On a root-complex port, its link index is always 0.
	2. On a endpoint port, it is the number in the last column of file
	   /proc/driver/gxpci_ep_major_mac_link.

