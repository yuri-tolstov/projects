/**********************************************************************
 * Author: Cavium Inc. 
 *
 * Contact: support@cavium.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2007 Cavium Inc. 
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Inc. for more information
**********************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/phy.h>
#include <linux/of_net.h>

#include <net/dst.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-pip.h>
#include <asm/octeon/cvmx-pko.h>
#include <asm/octeon/cvmx-fau.h>
#include <asm/octeon/cvmx-ipd.h>
#include <asm/octeon/cvmx-srio.h>
#include <asm/octeon/cvmx-helper.h>

#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-smix-defs.h>

#include "ethernet-defines.h"
#include "octeon-ethernet.h"
#include "ethernet-proc.h"
#include "ethernet-mem.h"
#include "ethernet-rx.h"
#include "ethernet-tx.h"
#include "ethernet-mdio.h"
#include "ethernet-util.h"


#if defined(CONFIG_OCTEON_NUM_PACKET_BUFFERS) \
	&& CONFIG_OCTEON_NUM_PACKET_BUFFERS
int num_packet_buffers = CONFIG_OCTEON_NUM_PACKET_BUFFERS;
#else
int num_packet_buffers = 1024;
#endif
module_param(num_packet_buffers, int, 0444);
MODULE_PARM_DESC(num_packet_buffers, "\n"
	"\tNumber of packet buffers to allocate and store in the\n"
	"\tFPA. By default, 1024 packet buffers are used unless\n"
	"\tCONFIG_OCTEON_NUM_PACKET_BUFFERS is defined.");

int pow_receive_group = 15;
module_param(pow_receive_group, int, 0444);
MODULE_PARM_DESC(pow_receive_group, "\n"
       "\tPOW group to receive packets from. All ethernet hardware\n"
       "\twill be configured to send incomming packets to this POW\n"
       "\tgroup. Also any other software can submit packets to this\n"
       "\tgroup for the kernel to process.");

static int disable_core_queueing = 1;
module_param(disable_core_queueing, int, 0444);
MODULE_PARM_DESC(disable_core_queueing, "\n"
	"\tWhen set the networking core's tx_queue_len is set to zero.  This\n"
	"\tallows packets to be sent without lock contention in the packet scheduler\n"
	"\tresulting in some cases in improved throughput.");

int max_rx_cpus = -1;
module_param(max_rx_cpus, int, 0444);
MODULE_PARM_DESC(max_rx_cpus, "\n"
	"\tThe maximum number of CPUs to use for packet reception.\n"
	"\tUse -1 to use all available CPUs.");

int rx_napi_weight = 32;
module_param(rx_napi_weight, int, 0444);
MODULE_PARM_DESC(rx_napi_weight, "\n"
	"\tThe NAPI WEIGHT parameter.");

/*Autonegotiation mode (See octeon-ethernet.h)*/
int aneg_mode = ANEG_MODE_SGMII_MAC;
module_param(aneg_mode, int, 0444);
MODULE_PARM_DESC(aneg_mode,"\n"
	"\tAutonegotiation Mode\n"
	"\t0 = SGMII:MAC, 1 = SGMII:PHY, 2 = 1000BASE-X");

int cvm_oct_tx_wqe_pool = -1;

/**
 * cvm_oct_poll_queue - Workqueue for polling operations.
 */
struct workqueue_struct *cvm_oct_poll_queue;

/**
 * cvm_oct_poll_queue_stopping - flag to indicate polling should stop.
 *
 * Set to one right before cvm_oct_poll_queue is destroyed.
 */
atomic_t cvm_oct_poll_queue_stopping = ATOMIC_INIT(0);

/*
 * cvm_oct_by_pkind is an array of every ethernet device owned by this
 * driver indexed by the IPD pkind/port_number.  If an entry is empty
 * (NULL) it either doesn't exist, or there was a collision.  The two
 * cases can be distinguished by trying to look up via
 * cvm_oct_dev_for_port();
 */
struct octeon_ethernet *cvm_oct_by_pkind[64] __cacheline_aligned;

/*
 * cvm_oct_by_srio_mbox is indexed by the SRIO mailbox.
 */
struct octeon_ethernet *cvm_oct_by_srio_mbox[4][4];

/*
 * cvm_oct_list is a list of all cvm_oct_private_t created by this driver.
 */
LIST_HEAD(cvm_oct_list);

static void cvm_oct_rx_refill_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(cvm_oct_rx_refill_work, cvm_oct_rx_refill_worker);

static void cvm_oct_rx_refill_worker(struct work_struct *work)
{
	/*
	 * FPA 0 may have been drained, try to refill it if we need
	 * more than num_packet_buffers / 2, otherwise normal receive
	 * processing will refill it.  If it were drained, no packets
	 * could be received so cvm_oct_napi_poll would never be
	 * invoked to do the refill.
	 */
	cvm_oct_rx_refill_pool(num_packet_buffers / 2);

	if (!atomic_read(&cvm_oct_poll_queue_stopping))
		queue_delayed_work(cvm_oct_poll_queue,
				   &cvm_oct_rx_refill_work, HZ);
}

static void cvm_oct_periodic_worker(struct work_struct *work)
{
	struct octeon_ethernet *priv = container_of(work,
						    struct octeon_ethernet,
						    port_periodic_work.work);
	if (priv->poll)
		priv->poll(priv->netdev);

	priv->netdev->netdev_ops->ndo_get_stats(priv->netdev);

	if (!atomic_read(&cvm_oct_poll_queue_stopping))
		queue_delayed_work(cvm_oct_poll_queue, &priv->port_periodic_work, HZ);
 }

static int cvm_oct_num_output_buffers;

static __init void cvm_oct_configure_common_hw(void)
{
	/* Setup the FPA */
	cvmx_fpa_enable();

	if (cvm_oct_alloc_fpa_pool(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE) < 0) {
		pr_err("cvm_oct_alloc_fpa_pool(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE) failed.\n");
		return;
	}
	cvm_oct_mem_fill_fpa(CVMX_FPA_PACKET_POOL, num_packet_buffers);

	if (cvm_oct_alloc_fpa_pool(CVMX_FPA_WQE_POOL, CVMX_FPA_WQE_POOL_SIZE) < 0) {
		pr_err("cvm_oct_alloc_fpa_pool(CVMX_FPA_WQE_POOL, CVMX_FPA_WQE_POOL_SIZE) failed.\n");
		return;
	}
	cvm_oct_mem_fill_fpa(CVMX_FPA_WQE_POOL, num_packet_buffers);

	if (CVMX_FPA_OUTPUT_BUFFER_POOL != CVMX_FPA_PACKET_POOL) {
		cvm_oct_num_output_buffers = 4 * octeon_pko_get_total_queues();
		if (cvm_oct_alloc_fpa_pool(CVMX_FPA_OUTPUT_BUFFER_POOL, CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE) < 0) {
			pr_err("cvm_oct_alloc_fpa_pool(CVMX_FPA_OUTPUT_BUFFER_POOL, CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE) failed.\n");
			return;
		}
		cvm_oct_mem_fill_fpa(CVMX_FPA_OUTPUT_BUFFER_POOL, cvm_oct_num_output_buffers);
	}

	cvm_oct_tx_wqe_pool = cvm_oct_alloc_fpa_pool(-1, CVMX_FPA_WQE_POOL_SIZE);

	if (cvm_oct_tx_wqe_pool < 0)
		pr_err("cvm_oct_alloc_fpa_pool(-1, CVMX_FPA_WQE_POOL_SIZE)) failed.\n");
}

/**
 * cvm_oct_register_callback -  Register a intercept callback for the named device.
 *
 * It returns the net_device structure for the ethernet port. Usign a
 * callback of NULL will remove the callback. Note that this callback
 * must not disturb scratch. It will be called with SYNCIOBDMAs in
 * progress and userspace may be using scratch. It also must not
 * disturb the group mask.
 *
 * @device_name: Device name to register for. (Example: "eth0")
 * @callback: Intercept callback to set.
 *
 * Returns the net_device structure for the ethernet port or NULL on failure.
 */
struct net_device *cvm_oct_register_callback(const char *device_name, cvm_oct_callback_t callback)
{
	struct octeon_ethernet *priv;

	list_for_each_entry(priv, &cvm_oct_list, list) {
		if (strcmp(device_name, priv->netdev->name) == 0) {
			priv->intercept_cb = callback;
			wmb();
			return priv->netdev;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(cvm_oct_register_callback);

/**
 * cvm_oct_free_work- Free a work queue entry
 *
 * @work_queue_entry: Work queue entry to free
 *
 * Returns Zero on success, Negative on failure.
 */
int cvm_oct_free_work(void *work_queue_entry)
{
	cvmx_wqe_t *work = work_queue_entry;

	int segments = work->word2.s.bufs;
	union cvmx_buf_ptr segment_ptr = work->packet_ptr;

	while (segments--) {
		union cvmx_buf_ptr next_ptr = *(union cvmx_buf_ptr *)phys_to_virt(segment_ptr.s.addr - 8);
		if (unlikely(!segment_ptr.s.i))
			cvmx_fpa_free(cvm_oct_get_buffer_ptr(segment_ptr),
				      segment_ptr.s.pool,
				      DONT_WRITEBACK(CVMX_FPA_PACKET_POOL_SIZE / 128));
		segment_ptr = next_ptr;
	}
	cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, DONT_WRITEBACK(1));

	return 0;
}
EXPORT_SYMBOL(cvm_oct_free_work);

/* Lock to protect racy cvmx_pko_get_port_status() */
static DEFINE_SPINLOCK(cvm_oct_tx_stat_lock);

/**
 * cvm_oct_common_get_stats - get the low level ethernet statistics
 * @dev:    Device to get the statistics from
 *
 * Returns Pointer to the statistics
 */
static struct net_device_stats *cvm_oct_common_get_stats(struct net_device *dev)
{
	unsigned long flags;
	cvmx_pip_port_status_t rx_status;
	cvmx_pko_port_status_t tx_status;
	u64 current_tx_octets;
	u32 current_tx_packets;
	struct octeon_ethernet *priv = netdev_priv(dev);

	if (octeon_is_simulation()) {
		/* The simulator doesn't support statistics */
		memset(&rx_status, 0, sizeof(rx_status));
		memset(&tx_status, 0, sizeof(tx_status));
	} else {
		cvmx_pip_get_port_status(priv->ipd_port, 1, &rx_status);

		spin_lock_irqsave(&cvm_oct_tx_stat_lock, flags);
		cvmx_pko_get_port_status(priv->ipd_port, 0, &tx_status);
		current_tx_packets = tx_status.packets;
		current_tx_octets = tx_status.octets;
		/*
		 * The tx_packets counter is 32-bits as are all these
		 * variables.  No truncation necessary.
		 */
		tx_status.packets = current_tx_packets - priv->last_tx_packets;
		/*
		 * The tx_octets counter is only 48-bits, so we need
		 * to truncate in case there was a wrap-around
		 */
		tx_status.octets = (current_tx_octets - priv->last_tx_octets) & 0xffffffffffffull;
		priv->last_tx_packets = current_tx_packets;
		priv->last_tx_octets = current_tx_octets;
		spin_unlock_irqrestore(&cvm_oct_tx_stat_lock, flags);
	}

	dev->stats.rx_packets += rx_status.inb_packets;
	dev->stats.tx_packets += tx_status.packets;
	dev->stats.rx_bytes += rx_status.inb_octets;
	dev->stats.tx_bytes += tx_status.octets;
	dev->stats.multicast += rx_status.multicast_packets;
	dev->stats.rx_crc_errors += rx_status.inb_errors;
	dev->stats.rx_frame_errors += rx_status.fcs_align_err_packets;

	/*
	 * The drop counter must be incremented atomically since the
	 * RX tasklet also increments it.
	 */
	atomic64_add(rx_status.dropped_packets,
		     (atomic64_t *)&dev->stats.rx_dropped);

	return &dev->stats;
}

/**
 * cvm_oct_common_change_mtu - change the link MTU
 * @dev:     Device to change
 * @new_mtu: The new MTU
 *
 * Returns Zero on success
 */
static int cvm_oct_common_change_mtu(struct net_device *dev, int new_mtu)
{
	struct octeon_ethernet *priv = netdev_priv(dev);
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	int vlan_bytes = 4;
#else
	int vlan_bytes = 0;
#endif

	/*
	 * Limit the MTU to make sure the ethernet packets are between
	 * 64 bytes and 65535 bytes.
	 */
	if ((new_mtu + 14 + 4 + vlan_bytes < 64)
	    || (new_mtu + 14 + 4 + vlan_bytes > 65392)) {
		pr_err("MTU must be between %d and %d.\n",
		       64 - 14 - 4 - vlan_bytes, 65392 - 14 - 4 - vlan_bytes);
		return -EINVAL;
	}
	dev->mtu = new_mtu;

	if (priv->has_gmx_regs) {
		/* Add ethernet header and FCS, and VLAN if configured. */
		int max_packet = new_mtu + 14 + 4 + vlan_bytes;

		if (OCTEON_IS_MODEL(OCTEON_CN3XXX)
		    || OCTEON_IS_MODEL(OCTEON_CN58XX)) {
			/* Signal errors on packets larger than the MTU */
			cvmx_write_csr(CVMX_GMXX_RXX_FRM_MAX(priv->interface_port, priv->interface),
				       max_packet);
		} else {
			union cvmx_pip_prt_cfgx port_cfg;
			int offset = octeon_has_feature(OCTEON_FEATURE_PKND) ?
				cvmx_helper_get_pknd(priv->interface, priv->interface_port) :
				cvmx_helper_get_ipd_port(priv->interface, priv->interface_port);

			port_cfg.u64 = cvmx_read_csr(CVMX_PIP_PRT_CFGX(offset));
			if (port_cfg.s.maxerr_en) {
				/*
				 * Disable the PIP check as it can
				 * only be controlled over a group of
				 * ports, let the check be done in the
				 * GMX instead.
				 */
				port_cfg.s.maxerr_en = 0;
				cvmx_write_csr(CVMX_PIP_PRT_CFGX(offset), port_cfg.u64);
			}
		}
		/*
		 * Set the hardware to truncate packets larger than
		 * the MTU. The jabber register must be set to a
		 * multiple of 8 bytes, so round up.
		 */
		cvmx_write_csr(CVMX_GMXX_RXX_JABBER(priv->interface_port, priv->interface),
			       (max_packet + 7) & ~7u);
	}
	return 0;
}

/**
 * cvm_oct_common_set_multicast_list - set the multicast list
 * @dev:    Device to work on
 */
static void cvm_oct_common_set_multicast_list(struct net_device *dev)
{
	union cvmx_gmxx_prtx_cfg gmx_cfg;
	struct octeon_ethernet *priv = netdev_priv(dev);

	if (priv->has_gmx_regs) {
		union cvmx_gmxx_rxx_adr_ctl control;
		control.u64 = 0;
		control.s.bcst = 1;	/* Allow broadcast MAC addresses */

		if (dev->mc_list || (dev->flags & IFF_ALLMULTI) ||
		    (dev->flags & IFF_PROMISC))
			/* Force accept multicast packets */
			control.s.mcst = 2;
		else
			/* Force reject multicat packets */
			control.s.mcst = 1;

		if (dev->flags & IFF_PROMISC)
			/*
			 * Reject matches if promisc. Since CAM is
			 * shut off, should accept everything.
			 */
			control.s.cam_mode = 0;
		else
			/* Filter packets based on the CAM */
			control.s.cam_mode = 1;

		gmx_cfg.u64 =
		    cvmx_read_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface));
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface),
			       gmx_cfg.u64 & ~1ull);

		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CTL(priv->interface_port, priv->interface),
			       control.u64);
		if (dev->flags & IFF_PROMISC)
			cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN
				       (priv->interface_port, priv->interface), 0);
		else
			cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN
				       (priv->interface_port, priv->interface), 1);

		cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface),
			       gmx_cfg.u64);
	}
}

/**
 * cvm_oct_common_set_mac_address - set the hardware MAC address for a device
 * @dev:    The device in question.
 * @addr:   Address structure to change it too.

 * Returns Zero on success
 */
static int cvm_oct_common_set_mac_address(struct net_device *dev, void *addr)
{
	struct octeon_ethernet *priv = netdev_priv(dev);
	union cvmx_gmxx_prtx_cfg gmx_cfg;

	memcpy(dev->dev_addr, addr + 2, 6);

	if (priv->has_gmx_regs) {
		int i;
		u8 *ptr = addr;
		u64 mac = 0;
		for (i = 0; i < 6; i++)
			mac = (mac << 8) | (u64) (ptr[i + 2]);

		gmx_cfg.u64 =
		    cvmx_read_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface));
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface),
			       gmx_cfg.u64 & ~1ull);

		cvmx_write_csr(CVMX_GMXX_SMACX(priv->interface_port, priv->interface), mac);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM0(priv->interface_port, priv->interface), ptr[2]);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM1(priv->interface_port, priv->interface), ptr[3]);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM2(priv->interface_port, priv->interface), ptr[4]);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM3(priv->interface_port, priv->interface), ptr[5]);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM4(priv->interface_port, priv->interface), ptr[6]);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM5(priv->interface_port, priv->interface), ptr[7]);
		cvm_oct_common_set_multicast_list(dev);
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface), gmx_cfg.u64);
	}
	return 0;
}

/**
 * cvm_oct_common_init - per network device initialization
 * @dev:    Device to initialize
 *
 * Returns Zero on success
 */
int cvm_oct_common_init(struct net_device *dev)
{
	unsigned long flags;
	cvmx_pko_port_status_t tx_status;
	struct octeon_ethernet *priv = netdev_priv(dev);
	struct sockaddr sa = {0};
	const u8 *mac = NULL;

	if (priv->of_node)
		mac = of_get_mac_address(priv->of_node);

	if (mac)
		memcpy(sa.sa_data, mac, ETH_ALEN);
	else
		random_ether_addr(sa.sa_data);

	if (priv->num_tx_queues) {
		dev->features |= NETIF_F_SG | NETIF_F_FRAGLIST;
		if (USE_HW_TCPUDP_CHECKSUM)
			dev->features |= NETIF_F_IP_CSUM;
	}

	/* We do our own locking, Linux doesn't need to */
	dev->features |= NETIF_F_LLTX;
	SET_ETHTOOL_OPS(dev, &cvm_oct_ethtool_ops);

	cvm_oct_phy_setup_device(dev);
	dev->netdev_ops->ndo_set_mac_address(dev, &sa);
	dev->netdev_ops->ndo_change_mtu(dev, dev->mtu);

	spin_lock_irqsave(&cvm_oct_tx_stat_lock, flags);
	cvmx_pko_get_port_status(priv->ipd_port, 0, &tx_status);
	priv->last_tx_packets = tx_status.packets;
	priv->last_tx_octets = tx_status.octets;
	/*
	 * Zero out stats for port so we won't mistakenly show
	 * counters from the bootloader.
	 */
	memset(&dev->stats, 0, sizeof(struct net_device_stats));
	spin_unlock_irqrestore(&cvm_oct_tx_stat_lock, flags);

	return 0;
}

void cvm_oct_common_uninit(struct net_device *dev)
{
	struct octeon_ethernet *priv = netdev_priv(dev);

	if (priv->phydev)
		phy_disconnect(priv->phydev);
}

static const struct net_device_ops cvm_oct_npi_netdev_ops = {
	.ndo_init		= cvm_oct_common_init,
	.ndo_uninit		= cvm_oct_common_uninit,
	.ndo_start_xmit		= cvm_oct_xmit,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};

static const struct net_device_ops cvm_oct_xaui_netdev_ops = {
	.ndo_init		= cvm_oct_xaui_init,
	.ndo_uninit		= cvm_oct_xaui_uninit,
	.ndo_open		= cvm_oct_xaui_open,
	.ndo_stop		= cvm_oct_xaui_stop,
	.ndo_start_xmit		= cvm_oct_xmit,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_sgmii_netdev_ops = {
	.ndo_init		= cvm_oct_sgmii_init,
	.ndo_uninit		= cvm_oct_sgmii_uninit,
	.ndo_open		= cvm_oct_sgmii_open,
	.ndo_stop		= cvm_oct_sgmii_stop,
	.ndo_start_xmit		= cvm_oct_xmit,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_spi_netdev_ops = {
	.ndo_init		= cvm_oct_spi_init,
	.ndo_uninit		= cvm_oct_spi_uninit,
	.ndo_start_xmit		= cvm_oct_xmit,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_rgmii_netdev_ops = {
	.ndo_init		= cvm_oct_rgmii_init,
	.ndo_uninit		= cvm_oct_rgmii_uninit,
	.ndo_open		= cvm_oct_rgmii_open,
	.ndo_stop		= cvm_oct_rgmii_stop,
	.ndo_start_xmit		= cvm_oct_xmit,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
#ifdef CONFIG_OCTEON_ETHERNET_LOCKLESS_IF_SUPPORTED
static const struct net_device_ops cvm_oct_xaui_lockless_netdev_ops = {
	.ndo_init		= cvm_oct_xaui_init,
	.ndo_uninit		= cvm_oct_xaui_uninit,
	.ndo_open		= cvm_oct_xaui_open,
	.ndo_stop		= cvm_oct_xaui_stop,
	.ndo_start_xmit		= cvm_oct_xmit_lockless,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_sgmii_lockless_netdev_ops = {
	.ndo_init		= cvm_oct_sgmii_init,
	.ndo_uninit		= cvm_oct_sgmii_uninit,
	.ndo_open		= cvm_oct_sgmii_open,
	.ndo_stop		= cvm_oct_sgmii_stop,
	.ndo_start_xmit		= cvm_oct_xmit_lockless,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_spi_lockless_netdev_ops = {
	.ndo_init		= cvm_oct_spi_init,
	.ndo_uninit		= cvm_oct_spi_uninit,
	.ndo_start_xmit		= cvm_oct_xmit_lockless,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
static const struct net_device_ops cvm_oct_rgmii_lockless_netdev_ops = {
	.ndo_init		= cvm_oct_rgmii_init,
	.ndo_uninit		= cvm_oct_rgmii_uninit,
	.ndo_open		= cvm_oct_rgmii_open,
	.ndo_stop		= cvm_oct_rgmii_stop,
	.ndo_start_xmit		= cvm_oct_xmit_lockless,
	.ndo_set_multicast_list	= cvm_oct_common_set_multicast_list,
	.ndo_set_mac_address	= cvm_oct_common_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_common_change_mtu,
	.ndo_get_stats		= cvm_oct_common_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
#endif
#ifdef CONFIG_RAPIDIO
static const struct net_device_ops cvm_oct_srio_netdev_ops = {
	.ndo_init		= cvm_oct_srio_init,
	.ndo_start_xmit		= cvm_oct_xmit_srio,
	.ndo_set_mac_address	= cvm_oct_srio_set_mac_address,
	.ndo_do_ioctl		= cvm_oct_ioctl,
	.ndo_change_mtu		= cvm_oct_srio_change_mtu,
	.ndo_get_stats		= cvm_oct_srio_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= cvm_oct_poll_controller,
#endif
};
#endif

extern void octeon_mdiobus_force_mod_depencency(void);

static int num_devices_extra_wqe;
#define PER_DEVICE_EXTRA_WQE (MAX_OUT_QUEUE_DEPTH)

static void cvm_oct_override_pko_queue_priority(int pko_port, u64 priorities[16])
{
	int i;

	if (octeon_pko_lockless()) {
		for (i = 0; i < 16; i++)
			priorities[i] = 8;
	}
}

static struct rb_root cvm_oct_ipd_tree = RB_ROOT;

void cvm_oct_add_ipd_port(struct octeon_ethernet *port)
{
	struct rb_node **link = &cvm_oct_ipd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct octeon_ethernet *n;
	int value = port->key;

	while (*link) {
		parent = *link;
		n = rb_entry(parent, struct octeon_ethernet, ipd_tree);

		if (value < n->key)
			link = &(*link)->rb_left;
		else if (value > n->key)
			link = &(*link)->rb_right;
		else
			BUG();
	}
	rb_link_node(&port->ipd_tree, parent, link);
	rb_insert_color(&port->ipd_tree, &cvm_oct_ipd_tree);
}

struct octeon_ethernet *cvm_oct_dev_for_port(int port_number)
{
	struct rb_node *n = cvm_oct_ipd_tree.rb_node;
	while (n) {
		struct octeon_ethernet *s = rb_entry(n, struct octeon_ethernet, ipd_tree);

		if (s->key > port_number)
			n = n->rb_left;
		else if (s->key < port_number)
			n = n->rb_left;
		else
			return s;
	}
	return NULL;
}

static struct device_node * __init cvm_oct_of_get_child(const struct device_node *parent,
							int reg_val)
{
	struct device_node *node = NULL;
	int size;
	const __be32 *addr;

	for (;;) {
		node = of_get_next_child(parent, node);
		if (!node)
			break;
		addr = of_get_property(node, "reg", &size);
		if (addr && (be32_to_cpu(*addr) == reg_val))
			break;
	}
	return node;
}

static struct device_node * __init cvm_oct_node_for_port(struct device_node *pip,
							 int interface, int port)
{
	struct device_node *ni, *np;

	ni = cvm_oct_of_get_child(pip, interface);
	if (!ni)
		return NULL;

	np = cvm_oct_of_get_child(ni, port);
	of_node_put(ni);

	return np;
}

static int __init cvm_oct_get_port_status(struct device_node *pip)
{
	int i, j;
	int num_interfaces = cvmx_helper_get_number_of_interfaces();

	for (i = 0; i < num_interfaces; i++) {
		int num_ports = cvmx_helper_interface_enumerate(i);
		int mode = cvmx_helper_interface_get_mode(i);

		for (j = 0; j < num_ports; j++) {
			if (mode == CVMX_HELPER_INTERFACE_MODE_RGMII
			    || mode == CVMX_HELPER_INTERFACE_MODE_GMII
			    || mode == CVMX_HELPER_INTERFACE_MODE_XAUI
			    || mode == CVMX_HELPER_INTERFACE_MODE_RXAUI
			    || mode == CVMX_HELPER_INTERFACE_MODE_SGMII
			    || mode == CVMX_HELPER_INTERFACE_MODE_SPI) {
				if (cvm_oct_node_for_port(pip, i, j) != NULL)
					cvmx_helper_port_valid[i][j] = 1;
				else
					cvmx_helper_port_valid[i][j] = 0;
			} else
				cvmx_helper_port_valid[i][j] = 1;
		}
	}
	return 0;
}

static int __init cvm_oct_init_module(void)
{
	int num_interfaces;
	int interface;
	int fau = FAU_NUM_PACKET_BUFFERS_TO_FREE;
	int qos;
	struct device_node *pip;
	int i;
	int rv = 0;

	octeon_mdiobus_force_mod_depencency();
	pr_notice("octeon-ethernet %s\n", OCTEON_ETHERNET_VERSION);

	pip = of_find_node_by_path("pip");
	if (!pip) {
		pr_err("Error: No 'pip' in /aliases\n");
		return -EINVAL;
	}

	cvm_oct_get_port_status(pip);

	cvmx_override_pko_queue_priority = cvm_oct_override_pko_queue_priority;

	cvm_oct_poll_queue = create_singlethread_workqueue("octeon-ethernet");
	if (cvm_oct_poll_queue == NULL) {
		pr_err("octeon-ethernet: Cannot create workqueue");
		rv = -ENOMEM;
		goto err_cleanup;
	}

	cvm_oct_proc_initialize();
	cvm_oct_configure_common_hw();

	cvmx_helper_initialize_packet_io_global();

	/* Enable red after interface is initialized */
	if (USE_RED)
		cvmx_helper_setup_red(num_packet_buffers / 4,
				      num_packet_buffers / 8);


	/* Change the input group for all ports before input is enabled */
	num_interfaces = cvmx_helper_get_number_of_interfaces();
	for (interface = 0; interface < num_interfaces; interface++) {
		int num_ports = cvmx_helper_ports_on_interface(interface);
		int port;
		for (port = 0; port < num_ports; port++) {
			union cvmx_pip_prt_tagx pip_prt_tagx;
			int pkind;
			if (cvmx_helper_is_port_valid(interface, port) == 0)
				continue;

			if (octeon_has_feature(OCTEON_FEATURE_PKND))
				pkind = cvmx_helper_get_pknd(interface, port);
			else
				pkind = cvmx_helper_get_ipd_port(interface, port);

			pip_prt_tagx.u64 = cvmx_read_csr(CVMX_PIP_PRT_TAGX(pkind));
			pip_prt_tagx.s.grp = pow_receive_group;
			cvmx_write_csr(CVMX_PIP_PRT_TAGX(pkind), pip_prt_tagx.u64);
		}
	}

	cvmx_helper_ipd_and_packet_input_enable();

	/*
	 * Initialize the FAU used for counting packet buffers that
	 * need to be freed.
	 */
	cvmx_fau_atomic_write32(FAU_NUM_PACKET_BUFFERS_TO_FREE, 0);

	for (interface = 0; interface < num_interfaces; interface++) {
		cvmx_helper_interface_mode_t imode = cvmx_helper_interface_get_mode(interface);
		int num_ports = cvmx_helper_ports_on_interface(interface);
		int interface_port;

		if (imode == CVMX_HELPER_INTERFACE_MODE_SRIO)
			num_ports = 4;

		if (imode == CVMX_HELPER_INTERFACE_MODE_ILK)
			continue;

		for (interface_port = 0; num_ports > 0;
		     interface_port++, num_ports--) {
			struct octeon_ethernet *priv;
			int base_queue;
			struct net_device *dev = alloc_etherdev(sizeof(struct octeon_ethernet));
			if (!dev) {
				pr_err("Failed to allocate ethernet device for port %d:%d\n", interface, interface_port);
				continue;
			}

			if (cvmx_helper_is_port_valid(interface, interface_port) == 0)
				continue;

			if (disable_core_queueing)
				dev->tx_queue_len = 0;

			/* Initialize the device private structure. */
			priv = netdev_priv(dev);
			priv->of_node = cvm_oct_node_for_port(pip, interface, interface_port);
			RB_CLEAR_NODE(&priv->ipd_tree);
			priv->netdev = dev;
			priv->interface = interface;
			priv->interface_port = interface_port;

			INIT_DELAYED_WORK(&priv->port_periodic_work,
					  cvm_oct_periodic_worker);
			priv->imode = imode;

			if (imode == CVMX_HELPER_INTERFACE_MODE_SRIO) {
                                int mbox = cvmx_helper_get_ipd_port(interface, interface_port) - cvmx_helper_get_ipd_port(interface, 0);
				cvmx_srio_tx_message_header_t tx_header;
				tx_header.u64 = 0;
				tx_header.s.tt = 0;
				tx_header.s.ssize = 0xe;
				tx_header.s.mbox = mbox;
				tx_header.s.lns = 1;
                                tx_header.s.intr = 1;
				priv->srio_tx_header = tx_header.u64;
				priv->ipd_port = cvmx_helper_get_ipd_port(interface, mbox >> 1);
				priv->pko_port = priv->ipd_port;
				priv->key = priv->ipd_port + (0x10000 * mbox);
				base_queue = cvmx_pko_get_base_queue(priv->ipd_port) + (mbox & 1);
				priv->num_tx_queues = 1;
				cvm_oct_by_srio_mbox[interface - 4][mbox] = priv;
			} else {
				priv->ipd_port = cvmx_helper_get_ipd_port(interface, interface_port);
				priv->key = priv->ipd_port;
				priv->pko_port = cvmx_helper_get_pko_port(interface, interface_port);
				base_queue = cvmx_pko_get_base_queue(priv->ipd_port);
				priv->num_tx_queues = cvmx_pko_get_num_queues(priv->ipd_port);
			}

			BUG_ON(priv->num_tx_queues < 1);
			BUG_ON(priv->num_tx_queues > 32);

			if (octeon_has_feature(OCTEON_FEATURE_PKND))
				priv->ipd_pkind = cvmx_helper_get_pknd(interface, interface_port);
			else
				priv->ipd_pkind = priv->ipd_port;

			for (qos = 0; qos < priv->num_tx_queues; qos++) {
				priv->tx_queue[qos].queue = base_queue + qos;
				fau = fau - sizeof(u32);
				priv->tx_queue[qos].fau = fau;
				cvmx_fau_atomic_write32(priv->tx_queue[qos].fau, 0);
			}

			/* Cache the fact that there may be multiple queues */
			priv->tx_multiple_queues =
				(CVMX_PKO_QUEUES_PER_PORT_INTERFACE0 > 1) ||
				(CVMX_PKO_QUEUES_PER_PORT_INTERFACE1 > 1);

			switch (priv->imode) {

			/* These types don't support ports to IPD/PKO */
			case CVMX_HELPER_INTERFACE_MODE_DISABLED:
			case CVMX_HELPER_INTERFACE_MODE_PCIE:
			case CVMX_HELPER_INTERFACE_MODE_PICMG:
				break;

			case CVMX_HELPER_INTERFACE_MODE_NPI:
				dev->netdev_ops = &cvm_oct_npi_netdev_ops;
				strcpy(dev->name, "npi%d");
				break;

			case CVMX_HELPER_INTERFACE_MODE_XAUI:
			case CVMX_HELPER_INTERFACE_MODE_RXAUI:
#ifdef CONFIG_OCTEON_ETHERNET_LOCKLESS_IF_SUPPORTED
				if (octeon_pko_lockless()) {
					dev->netdev_ops = &cvm_oct_xaui_lockless_netdev_ops;
					priv->tx_lockless = 1;
				} else
#endif
					dev->netdev_ops = &cvm_oct_xaui_netdev_ops;
				priv->has_gmx_regs = 1;
				strcpy(dev->name, "xaui%d");
				break;

			case CVMX_HELPER_INTERFACE_MODE_LOOP:
				dev->netdev_ops = &cvm_oct_npi_netdev_ops;
				strcpy(dev->name, "loop%d");
				break;

			case CVMX_HELPER_INTERFACE_MODE_SGMII:
#ifdef CONFIG_OCTEON_ETHERNET_LOCKLESS_IF_SUPPORTED
				if (octeon_pko_lockless()) {
					dev->netdev_ops = &cvm_oct_sgmii_lockless_netdev_ops;
					priv->tx_lockless = 1;
				} else
#endif
					dev->netdev_ops = &cvm_oct_sgmii_netdev_ops;
				priv->has_gmx_regs = 1;
				strcpy(dev->name, "eth%d");
				break;

			case CVMX_HELPER_INTERFACE_MODE_SPI:
#ifdef CONFIG_OCTEON_ETHERNET_LOCKLESS_IF_SUPPORTED
				if (octeon_pko_lockless()) {
					dev->netdev_ops = &cvm_oct_spi_lockless_netdev_ops;
					priv->tx_lockless = 1;
				} else
#endif
					dev->netdev_ops = &cvm_oct_spi_netdev_ops;
				strcpy(dev->name, "spi%d");
				break;

			case CVMX_HELPER_INTERFACE_MODE_RGMII:
			case CVMX_HELPER_INTERFACE_MODE_GMII:
#ifdef CONFIG_OCTEON_ETHERNET_LOCKLESS_IF_SUPPORTED
				if (octeon_pko_lockless()) {
					dev->netdev_ops = &cvm_oct_rgmii_lockless_netdev_ops;
					priv->tx_lockless = 1;
				} else
#endif
					dev->netdev_ops = &cvm_oct_rgmii_netdev_ops;
				priv->has_gmx_regs = 1;
				strcpy(dev->name, "eth%d");
				break;
#ifdef CONFIG_RAPIDIO
			case CVMX_HELPER_INTERFACE_MODE_SRIO:
				dev->netdev_ops = &cvm_oct_srio_netdev_ops;
				strcpy(dev->name, "rio%d");
				break;
#endif
			}

			netif_carrier_off(dev);
			if (!dev->netdev_ops) {
				free_netdev(dev);
			} else if (register_netdev(dev) < 0) {
				pr_err("Failed to register ethernet device for interface %d, port %d\n",
					 interface, priv->ipd_port);
				free_netdev(dev);
			} else {
				list_add_tail(&priv->list, &cvm_oct_list);
				if (cvm_oct_by_pkind[priv->ipd_pkind] == NULL)
					cvm_oct_by_pkind[priv->ipd_pkind] = priv;
				else
					cvm_oct_by_pkind[priv->ipd_pkind] = (void *)-1L;

				cvm_oct_add_ipd_port(priv);
				/*
				 * Each transmit queue will need its
				 * own MAX_OUT_QUEUE_DEPTH worth of
				 * WQE to track the transmit skbs.
				 */
				cvm_oct_mem_fill_fpa(cvm_oct_tx_wqe_pool, PER_DEVICE_EXTRA_WQE);
				num_devices_extra_wqe++;

				queue_delayed_work(cvm_oct_poll_queue,
                                                   &priv->port_periodic_work, HZ);
			}
		}
	}

	/* Set collision locations to NULL */
	for (i = 0; i < ARRAY_SIZE(cvm_oct_by_pkind); i++)
		if (cvm_oct_by_pkind[i] == (void *)-1L)
			cvm_oct_by_pkind[i] = NULL;

	cvm_oct_rx_initialize(num_packet_buffers + num_devices_extra_wqe * PER_DEVICE_EXTRA_WQE);

	queue_delayed_work(cvm_oct_poll_queue, &cvm_oct_rx_refill_work, HZ);

	return rv;
err_cleanup:
	cvm_oct_mem_cleanup();
	return rv;
}

static void __exit cvm_oct_cleanup_module(void)
{
	struct octeon_ethernet *priv;
	struct octeon_ethernet *tmp;



	/*
	 * Put both taking the interface down and unregistering it
	 * under the lock.  That way the devices cannot be taken back
	 * up in the middle of everything.
	 */
	rtnl_lock();

	/*
	 * Take down all the interfaces, this disables the GMX and
	 * prevents it from getting into a Bad State when IPD is
	 * disabled.
	 */
	list_for_each_entry(priv, &cvm_oct_list, list) {
		unsigned int f = dev_get_flags(priv->netdev);
		dev_change_flags(priv->netdev, f & ~IFF_UP);
	}

	mdelay(10);

	cvmx_ipd_disable();

	mdelay(10);

	atomic_inc_return(&cvm_oct_poll_queue_stopping);
	cancel_delayed_work_sync(&cvm_oct_rx_refill_work);

	cvm_oct_rx_shutdown0();

	/* unregister the ethernet devices */
	list_for_each_entry(priv, &cvm_oct_list, list) {
		cancel_delayed_work_sync(&priv->port_periodic_work);
		unregister_netdevice(priv->netdev);
	}

	rtnl_unlock();

	/* Free the ethernet devices */
	list_for_each_entry_safe_reverse(priv, tmp, &cvm_oct_list, list) {
		list_del(&priv->list);
		free_netdev(priv->netdev);
	}

	cvmx_helper_shutdown_packet_io_global();

	cvm_oct_rx_shutdown1();

	destroy_workqueue(cvm_oct_poll_queue);

	cvm_oct_proc_shutdown();

	/* Free the HW pools */
	cvm_oct_mem_empty_fpa(CVMX_FPA_PACKET_POOL, num_packet_buffers);
	cvm_oct_release_fpa_pool(CVMX_FPA_PACKET_POOL);

	cvm_oct_mem_empty_fpa(CVMX_FPA_WQE_POOL, num_packet_buffers);
	cvm_oct_release_fpa_pool(CVMX_FPA_WQE_POOL);

	cvm_oct_mem_empty_fpa(cvm_oct_tx_wqe_pool, num_devices_extra_wqe * PER_DEVICE_EXTRA_WQE);
	cvm_oct_release_fpa_pool(cvm_oct_tx_wqe_pool);

	if (CVMX_FPA_OUTPUT_BUFFER_POOL != CVMX_FPA_PACKET_POOL) {
		cvm_oct_mem_empty_fpa(CVMX_FPA_OUTPUT_BUFFER_POOL, cvm_oct_num_output_buffers);
		cvm_oct_release_fpa_pool(CVMX_FPA_OUTPUT_BUFFER_POOL);
	}

	/*
	 * Disable FPA, all buffers are free, not done by helper
	 * shutdown.  But don't do it on 68XX as this causes SSO to
	 * malfunction on subsequent initialization.
	 */
	if (!OCTEON_IS_MODEL(OCTEON_CN68XX))
		cvmx_fpa_disable();

	cvm_oct_mem_cleanup();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cavium Inc. <support@cavium.com>");
MODULE_DESCRIPTION("Cavium Inc. OCTEON ethernet driver.");
#ifdef MODULE
module_init(cvm_oct_init_module);
#else
late_initcall(cvm_oct_init_module);
#endif
module_exit(cvm_oct_cleanup_module);


