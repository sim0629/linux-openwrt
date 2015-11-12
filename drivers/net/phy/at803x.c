/*
 * drivers/net/phy/at803x.c
 *
 * Driver for Atheros 803x PHY
 *
 * Author: Matus Ujhelyi <ujhelyi.m@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_data/phy-at803x.h>

#define AT803X_INTR_ENABLE			0x12
#define AT803X_INTR_STATUS			0x13
#define AT803X_WOL_ENABLE			0x01
#define AT803X_DEVICE_ADDR			0x03
#define AT803X_LOC_MAC_ADDR_0_15_OFFSET		0x804C
#define AT803X_LOC_MAC_ADDR_16_31_OFFSET	0x804B
#define AT803X_LOC_MAC_ADDR_32_47_OFFSET	0x804A
#define AT803X_MMD_ACCESS_CONTROL		0x0D
#define AT803X_MMD_ACCESS_CONTROL_DATA		0x0E
#define AT803X_FUNC_DATA			0x4003

#define AT803X_PCS_SMART_EEE_CTRL3		0x805D

#define AT803X_SMART_EEE_CTRL3_LPI_TX_DELAY_SEL_MASK   0x3
#define AT803X_SMART_EEE_CTRL3_LPI_TX_DELAY_SEL_SHIFT  12
#define AT803X_SMART_EEE_CTRL3_LPI_EN                  BIT(8)

#define AT803X_DEBUG_PORT_ACCESS_OFFSET		0x1D
#define AT803X_DEBUG_PORT_ACCESS_DATA		0x1E

#define AT803X_DBG0_REG				0x00
#define AT803X_DBG0_RGMII_RX_CLK_DELAY_EN	BIT(8)

#define AT803X_DBG5_REG				0x05
#define AT803X_DBG5_RGMII_TX_CLK_DELAY_EN	BIT(8)

MODULE_DESCRIPTION("Atheros 803x PHY driver");
MODULE_AUTHOR("Matus Ujhelyi");
MODULE_LICENSE("GPL");

static u16
at803x_dbg_reg_rmw(struct phy_device *phydev, u16 reg, u16 clear, u16 set)
{
	struct mii_bus *bus = phydev->bus;
	int val;

	mutex_lock(&bus->mdio_lock);

	bus->write(bus, phydev->addr, AT803X_DEBUG_PORT_ACCESS_OFFSET, reg);
	val = bus->read(bus, phydev->addr, AT803X_DEBUG_PORT_ACCESS_DATA);
	if (val < 0) {
		val = 0xffff;
		goto out;
	}

	val &= ~clear;
	val |= set;
	bus->write(bus, phydev->addr, AT803X_DEBUG_PORT_ACCESS_DATA, val);

out:
	mutex_unlock(&bus->mdio_lock);
	return val;
}

static inline void
at803x_dbg_reg_set(struct phy_device *phydev, u16 reg, u16 set)
{
	at803x_dbg_reg_rmw(phydev, reg, 0, set);
}

static inline void
at803x_dbg_reg_clr(struct phy_device *phydev, u16 reg, u16 clear)
{
	at803x_dbg_reg_rmw(phydev, reg, clear, 0);
}

static void at803x_set_wol_mac_addr(struct phy_device *phydev)
{
	struct net_device *ndev = phydev->attached_dev;
	const u8 *mac;
	unsigned int i, offsets[] = {
		AT803X_LOC_MAC_ADDR_32_47_OFFSET,
		AT803X_LOC_MAC_ADDR_16_31_OFFSET,
		AT803X_LOC_MAC_ADDR_0_15_OFFSET,
	};

	if (!ndev)
		return;

	mac = (const u8 *) ndev->dev_addr;

	if (!is_valid_ether_addr(mac))
		return;

	for (i = 0; i < 3; i++) {
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_DEVICE_ADDR);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  offsets[i]);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_FUNC_DATA);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  mac[(i * 2) + 1] | (mac[(i * 2)] << 8));
	}
}

static void at803x_disable_smarteee(struct phy_device *phydev)
{
	phy_write_mmd(phydev, MDIO_MMD_PCS, AT803X_PCS_SMART_EEE_CTRL3,
		      1 << AT803X_SMART_EEE_CTRL3_LPI_TX_DELAY_SEL_SHIFT);
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV, 0);
}

static int at803x_config_init(struct phy_device *phydev)
{
	struct at803x_platform_data *pdata;
	int val;
	u32 features;
	int status;

	features = SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_AUI |
		   SUPPORTED_FIBRE | SUPPORTED_BNC;

	val = phy_read(phydev, MII_BMSR);
	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;
	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);
		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
	}

	phydev->supported = features;
	phydev->advertising = features;

	/* enable WOL */
	at803x_set_wol_mac_addr(phydev);
	status = phy_write(phydev, AT803X_INTR_ENABLE, AT803X_WOL_ENABLE);
	status = phy_read(phydev, AT803X_INTR_STATUS);

	pdata = dev_get_platdata(&phydev->dev);
	if (pdata) {
		if (pdata->disable_smarteee)
			at803x_disable_smarteee(phydev);

		if (pdata->enable_rgmii_rx_delay)
			at803x_dbg_reg_set(phydev, AT803X_DBG0_REG,
					   AT803X_DBG0_RGMII_RX_CLK_DELAY_EN);
		else
			at803x_dbg_reg_clr(phydev, AT803X_DBG0_REG,
					   AT803X_DBG0_RGMII_RX_CLK_DELAY_EN);

		if (pdata->enable_rgmii_tx_delay)
			at803x_dbg_reg_set(phydev, AT803X_DBG5_REG,
					   AT803X_DBG5_RGMII_TX_CLK_DELAY_EN);
		else
			at803x_dbg_reg_clr(phydev, AT803X_DBG5_REG,
					   AT803X_DBG5_RGMII_TX_CLK_DELAY_EN);
	}

	return 0;
}

/* ATHEROS 8035 */
static struct phy_driver at8035_driver = {
	.phy_id		= 0x004dd072,
	.name		= "Atheros 8035 ethernet",
	.phy_id_mask	= 0xffffffef,
	.config_init	= at803x_config_init,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
};

/* ATHEROS 8030 */
static struct phy_driver at8030_driver = {
	.phy_id		= 0x004dd076,
	.name		= "Atheros 8030 ethernet",
	.phy_id_mask	= 0xffffffef,
	.config_init	= at803x_config_init,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
};

static int __init atheros_init(void)
{
	int ret;

	ret = phy_driver_register(&at8035_driver);
	if (ret)
		goto err1;

	ret = phy_driver_register(&at8030_driver);
	if (ret)
		goto err2;

	return 0;

err2:
	phy_driver_unregister(&at8035_driver);
err1:
	return ret;
}

static void __exit atheros_exit(void)
{
	phy_driver_unregister(&at8035_driver);
	phy_driver_unregister(&at8030_driver);
}

module_init(atheros_init);
module_exit(atheros_exit);

static struct mdio_device_id __maybe_unused atheros_tbl[] = {
	{ 0x004dd076, 0xffffffef },
	{ 0x004dd072, 0xffffffef },
	{ }
};

MODULE_DEVICE_TABLE(mdio, atheros_tbl);
