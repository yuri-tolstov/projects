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
#include <linux/netdevice.h>
#include <net/dst.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-helper.h>
#include <asm/octeon/cvmx-helper-board.h>
#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-pcsx-defs.h>
#include <asm/octeon/cvmx-smix-defs.h>
#include <asm/octeon/cvmx-mdio.h>

#include "ethernet-defines.h"
#include "octeon-ethernet.h"
#include "ethernet-util.h"
#include "ethernet-mdio.h"

static
void cvm_oct_sgmii_poll(struct net_device *dev)
{
	struct octeon_ethernet *priv = netdev_priv(dev);
	cvmx_helper_link_info_t link_info;

#if 1
printk("***sgmii_poll: port=%d\n", priv->ipd_port);
#endif
	link_info = cvmx_helper_link_get(priv->ipd_port);
	if (link_info.u64 == priv->link_info)
		return;

	link_info = cvmx_helper_link_autoconf(priv->ipd_port);
	priv->link_info = link_info.u64;

	/* Tell the core */
	cvm_oct_set_carrier(priv, link_info);
}

int cvm_oct_sgmii_open(struct net_device *dev)
{
	union cvmx_gmxx_prtx_cfg gmx_cfg;
	struct octeon_ethernet *priv = netdev_priv(dev);
	cvmx_helper_link_info_t link_info;
	int rv;

	rv = cvm_oct_phy_setup_device(dev);
	if (rv)
		return rv;

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface));
	gmx_cfg.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface), gmx_cfg.u64);

	if (octeon_is_simulation())
		return 0;

	if (priv->phydev) {
		int r = phy_read_status(priv->phydev);
		if (r == 0 && priv->phydev->link == 0)
			netif_carrier_off(dev);
		cvm_oct_adjust_link(dev);
	} else {
		link_info = cvmx_helper_link_get(priv->ipd_port);
		if (!link_info.s.link_up)
			netif_carrier_off(dev);
		priv->poll = cvm_oct_sgmii_poll;
		cvm_oct_sgmii_poll(dev);
	}
	return 0;
}

int cvm_oct_sgmii_stop(struct net_device *dev)
{
	union cvmx_gmxx_prtx_cfg gmx_cfg;
	struct octeon_ethernet *priv = netdev_priv(dev);
	cvmx_helper_link_info_t link_info;

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface));
	gmx_cfg.s.en = 0;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(priv->interface_port, priv->interface), gmx_cfg.u64);

	priv->poll = NULL;
	if (priv->phydev) {
		phy_disconnect(priv->phydev);
	}
	priv->phydev = NULL;

        if (priv->last_link) {
                link_info.u64 = 0;
                priv->last_link = 0;
                cvmx_helper_link_set(priv->ipd_port, link_info);
        }
	return 0;
}

int cvm_oct_sgmii_init(struct net_device *dev)
{
	// struct octeon_ethernet *priv = netdev_priv(dev);
	cvm_oct_common_init(dev);
	dev->netdev_ops->ndo_stop(dev);
	return 0;
}

void cvm_oct_sgmii_uninit(struct net_device *dev)
{
	cvm_oct_common_uninit(dev);
}
