Introduction
------------

This example shows how to transfer data between a tile application and
a x86 host application using the Gx PCIe Raw DMA API.

Using the gxpci library, the tile applications t2h.c and h2t.c
transmits and receives packets to and from the host, respectively.
Each program runs in a loop posting buffers and waits for
the data transfer completions. They break out of the loop either when
the channel is reset or when the specified number of packets have been
transferred.

The host program, host.c, performs the following functions:
1. Call the host Raw DMA driver to obtain the target DMA buffer's address
   and size.
2. Map the aforementioned DMA buffer and the Raw DMA queue status to
   the host user space so that host application can access the buffer
   directly, for convenient data exchanges with the tile without making
   system calls. The queue status is monitored by the host application
   to find out if the tile application has exited or crashed.
3. Optionally, map the tile-side flow-control register to the host
   application so that it can update the tile side for FC purpose.
4. Activate the Raw DMA queue.

Running The Example
-------------------

To run the example, set the TILERA_ROOT environment variable and then do:

1. Upload tile_h2t and/or tile_t2h to the target system. The application
   will spawn N number of threads, where N is the number of Raw DMA queues,
   defined by GXPCI_RAW_DMA_QUEUE_COUNT in tilegxpci.h with the default
   value of 8.
2. Run host_h2t and/or host_t2h on the host. Since this application opens
   a single queue per instance, multiple copies need to be opened in parallel
   as in "host_t2h --queue_index=N &" where N is between 0 and 
   (GXPCI_RAW_DMA_QUEUE_COUNT - 1). For example,
	#!/bin/sh
	for chan in $(seq 0 7); do
	   host_t2h --queue_index=$chan &
	done

They will run the host-to-tile tests and tile-to-host tests, respectively,
reporting the total number of packets transferred, packet size, bits per
second and number of packets per second.

Note:
1. The tile examples only run on Gx chips which are connected to a x86 host.
2. If there are more than one Gx chip connected to a x86 host, select one
to test first by running 'setenv TILERA_DEV gxpciN', where N is the Gx port
index, i.e. '/dev/tilegxpciN'; then update card_index in host.c to N also.
3. If there are more than one Gx endpoint port on a single card such as a
Gx72-based PCIe card, change "/dev/tilegxpci%d/..." to
"/dev/tilegxpci%d-link1/..." in host.c to run the examples on the 2nd port.

