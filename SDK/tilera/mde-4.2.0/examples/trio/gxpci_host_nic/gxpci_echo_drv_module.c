/*
 * Copyright 2013 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * gxpci_echo_drv_module.c - Tilera Gx simple echo server on host
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/kthread.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
#include <net/net_namespace.h>
#endif

#if defined(TILEPCI_HOST)
#include "tilegxpci.h"
#else
#include <asm/tilegxpci.h>
#endif

/* This is the interval between the checks for the NIC up status. */
#define GXPCI_ECHO_SETUP_TIMEOUT_SEC	2

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
static struct delayed_work echo_dev_setup_work[GXPCI_HOST_NIC_COUNT];
#else
static struct work_struct echo_dev_setup_work[GXPCI_HOST_NIC_COUNT];
#endif

static struct net_device *net_devs[GXPCI_HOST_NIC_COUNT];

/*
 * Link index of the Gx PCIe end-point board connected to the x86 Linux host,
 * starting at 1 and set to 1 by default.
 */
static unsigned int link_index = 1;
module_param(link_index, uint, 0644);
MODULE_PARM_DESC(link_index, "Link index of the Gx PCIe end-point board.");

/* 
 * This is the egress function that transmits the packets to the PCIe
 * NIC interface. Note that packets could be dropped by dev_queue_xmit()
 * for egress congestion control.
 */
void
gxpci_echo_xmit(struct sk_buff *skb)
{
	dev_queue_xmit(skb);
}

/* 
 * This is the packet handler that gets called by the network ingress layer
 * to process the PCIe packets.
 */
static int
gxpci_echo_rcv(struct sk_buff *skb, struct net_device *ifp,
	       struct packet_type *pt, struct net_device *orig_dev)
{
	gxpci_echo_xmit(skb);

	return 0;
}

/* 
 * Function that runs periodically to check if the PCIe NIC connection is up.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
static void gxpci_echo_worker(struct work_struct *work)
#else
static void gxpci_echo_worker(void *work)
#endif
{
	struct net_device *net_dev;
	char ifname[IFNAMSIZ];
	unsigned long if_num;
	int err;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
	struct delayed_work *my_work =
		container_of(work, struct delayed_work, work);
	if_num = my_work - &echo_dev_setup_work[0];
#else
	if_num = (unsigned long)work;
#endif

	if (!net_devs[if_num]) {
		sprintf(ifname, "gxpci%d-%lu", link_index, if_num);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
		net_devs[if_num] = dev_get_by_name(&init_net, ifname);
#else
		net_devs[if_num] = dev_get_by_name(ifname);
#endif
		if (!net_devs[if_num]) {
			printk(KERN_WARNING "gxpci_echo: %s doesn't exist.\n",
				ifname);
			return;
		}
	}

	net_dev = net_devs[if_num];

	rtnl_lock();
	err = dev_open(net_dev);
	rtnl_unlock();

	if (err == 0) {
		printk(KERN_DEBUG "gxpci_echo: %s opened.\n", net_dev->name);

		return;
	}

	schedule_delayed_work(&echo_dev_setup_work[if_num],
					GXPCI_ECHO_SETUP_TIMEOUT_SEC * HZ);

	return;
};

static struct packet_type gxpci_pt __read_mostly = {
	.type = 0xABCD,
	.func = gxpci_echo_rcv,
};

static void
gxpci_echo_exit(void)
{
	int i;

	for (i = 0; i < GXPCI_HOST_NIC_COUNT; i++) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
		cancel_delayed_work_sync(&echo_dev_setup_work[i]);
#else
		cancel_delayed_work(&echo_dev_setup_work[i]);
#endif
		rtnl_lock();
		dev_put(net_devs[i]);
		rtnl_unlock();
	}

	dev_remove_pack(&gxpci_pt);
}

static int __init
gxpci_echo_init(void)
{
	long i;

	dev_add_pack(&gxpci_pt);

	/*
	 * Create a scheduled work to manage the PCIe net device setup.
	 */
	for (i = 0; i < GXPCI_HOST_NIC_COUNT; i++) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
		INIT_DELAYED_WORK(&echo_dev_setup_work[i], gxpci_echo_worker);
#else
		INIT_WORK(&echo_dev_setup_work[i], gxpci_echo_worker,
								(void *)i);
#endif
		schedule_delayed_work(&echo_dev_setup_work[i],
					GXPCI_ECHO_SETUP_TIMEOUT_SEC * HZ);
	}

	return 0;
}

module_init(gxpci_echo_init);
module_exit(gxpci_echo_exit);
