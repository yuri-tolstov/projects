This example shows how to transfer data between a tile application and
a x86 host kernel module.

Using the gxpci library, the tile application bench.c creates two
threads, i.e. a sender and a receiver. The sender thread transmits
packets to the host which returns the packets back to the tile,
to be consumed by the receiver thread.

The test kernel module, gxpci_echo_drv_module.c, listens on the 4
virtual NICs created by the Gx host PCIe driver and transmits
every ingress packet out of the same NIC. The NIC statistics and status
can be observed with command "ifconfig -a" on the host, with the
PCIe NICs having the name format gxpciN-M where N is the Gx PCIe link 
number starting at 1 and M is the NIC port number on this PCIe link
in the range of [0, 3].

Note that, due to module dependency, this test kernel module must be
loaded after the host driver is loaded and removed before the host 
driver is removed.

Note that this example only runs on Gx chips which are connected to
a x86 host.

Here are the steps to run the test:

1. Build the test kernel module on the host as follows:
	1) Copy file gxpci_echo_drv_module.c to directory
	   $TILERA_ROOT/lib/modules/gpl_src/pcie/
	2) Add the following two lines to the Makefile
	   $TILERA_ROOT/lib/modules/gpl_src/pcie/Makefile
	   after the line starting "tilegxpci-objs := \":
	   	obj-m  += gxpci_echo.o
	   	gxpci_echo-objs := gxpci_echo_drv_module.o
	3) Compile the module with command:
	   $TILERA_ROOT/lib/modules/tilepci-compile
	4) The module binary file gxpci_echo.ko is created in directory:
	   $TILERA_ROOT/lib/modules/`uname -r`-`uname -m`/
2. Boot the Gx PCIe end-point card using tile-monitor and upload
   the "bench" test program to the Gx board.
3. On the host, load the gxpci_echo module with command:
   insmod $TILERA_ROOT/lib/modules/`uname -r`-`uname -m`/gxpci_echo.ko
   Note that if there are more than one Gx PCIe end-point card connected to
   the host, user can specify a card to test by applying a kenerl module
   parameter "link_index=N" when loading the gxpci_echo module, where N is
   the Gx PCIe link number starting at 1.
4. On the tile, run the "bench" program.

On the host, you can use the ifconfig command to get the PCIe NIC's
networking stats. Note that when the bench program exits it passes 
the PCIe channel reset event to the host driver which in turn
calls the dev_close() on the NIC, i.e. effectively "ifconfig down".
To bring up the NIC interface, load the gxpci_echo module
again, as describe below.

This test has a few caveats:
	* Due to the fact that packets are often dropped at the host
	  because the host xmit can't keep up with the ingress, the 
	  tile receiver thread cannot receive all the packets that 
	  are generated by the tile sender. To ensure that the test
	  always returns, it is implemented in such a way that the
	  sender thread will make the receiver thread return, once the
	  sender finishes all the packets. This explains why the
	  receiver thread would often reports getting fewer number of
	  packets than the total number of packets.
	* The simple gxpci_echo module, a packet protocol handler,
	  is intended to show how to send and receive packets
	  through the PCIe NICs and its robustness is not the goal.
	  It opens the NIC once and does not poll for subsequent
	  connection events from the Gx which are issued by the "bench"
	  program. To run the test again, you need to unload and
	  re-load the gxpci_echo module again with commands:
	  > rmmod gxpci_echo
	  > insmod $TILERA_ROOT/lib/modules/`uname -r`-`uname -m`/gxpci_echo.ko
