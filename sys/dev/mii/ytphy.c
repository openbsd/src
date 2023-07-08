/*	$OpenBSD: ytphy.c,v 1.3 2023/07/08 08:18:30 kettenis Exp $	*/
/*
 * Copyright (c) 2001 Theo de Raadt
 * Copyright (c) 2023 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#ifdef __HAVE_FDT
#include <machine/fdt.h>
#include <dev/ofw/openfirm.h>
#endif

#define MII_MODEL_MOTORCOMM_YT8511	0x10
#define MII_MODEL_MOTORCOMM_YT8521	0x11

#define YT8511_REG_ADDR		0x1e
#define YT8511_REG_DATA		0x1f

#define YT8511_EXT_CLK_GATE		0x0c
#define  YT8511_TX_CLK_DELAY_SEL_MASK	(0xf << 4)
#define  YT8511_TX_CLK_DELAY_SEL_EN	(0xf << 4)
#define  YT8511_TX_CLK_DELAY_SEL_DIS	(0x2 << 4)
#define  YT8511_CLK_25M_SEL_MASK	(0x3 << 1)
#define  YT8511_CLK_25M_SEL_125M	(0x3 << 1)
#define  YT8511_RX_CLK_DELAY_EN		(1 << 0)
#define YT8511_EXT_DELAY_DRIVE		0x0d
#define  YT8511_TXC_DELAY_SEL_FE_MASK	(0xf << 12)
#define  YT8511_TXC_DELAY_SEL_FE_EN	(0xf << 12)
#define  YT8511_TXC_DELAY_SEL_FE_DIS	(0x2 << 12)
#define YT8511_EXT_SLEEP_CTRL		0x27
#define  YT8511_PLL_ON_IN_SLEEP		(1 << 14)

#define YT8521_EXT_CHIP_CONFIG		0xa001
#define  YT8521_RXC_DLY_EN		(1 << 8)
#define YT8521_EXT_RGMII_CONFIG1	0xa003
#define  YT8521_TX_CLK_SEL		(1 << 14)
#define  YT8521_RX_DELAY_SEL_MASK	(0xf << 10)
#define  YT8521_RX_DELAY_SEL_SHIFT	10
#define  YT8521_TX_DELAY_SEL_MASK	(0xf << 0)
#define  YT8521_TX_DELAY_SEL_SHIFT	0

int	ytphy_match(struct device *, void *, void *);
void	ytphy_attach(struct device *, struct device *, void *);

const struct cfattach ytphy_ca = {
	sizeof(struct mii_softc), ytphy_match, ytphy_attach, mii_phy_detach
};

struct cfdriver ytphy_cd = {
	NULL, "ytphy", DV_DULL
};

int	ytphy_service(struct mii_softc *, struct mii_data *, int);
void	ytphy_yt8511_init(struct mii_softc *);
void	ytphy_yt8521_init(struct mii_softc *);
void	ytphy_yt8521_update(struct mii_softc *);

const struct mii_phy_funcs ytphy_funcs = {
	ytphy_service, ukphy_status, mii_phy_reset,
};

static const struct mii_phydesc ytphys[] = {
	{ MII_OUI_MOTORCOMM,	MII_MODEL_MOTORCOMM_YT8531,
	  MII_STR_MOTORCOMM_YT8531 },
	{ 0,			0,
	  NULL },
};

int
ytphy_match(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;
	
	/*
	 * The MotorComm YT8511 and TY8521 have bogus MII OUIs, so
	 * match the complete ID including the rev.
	 */
	if (ma->mii_id1 == 0x0000 && ma->mii_id2 == 0x010a)
		return (10);
	if (ma->mii_id1 == 0x0000 && ma->mii_id2 == 0x011a)
		return (10);

	if (mii_phy_match(ma, ytphys) != NULL)
		return (10);

	return (0);
}

void
ytphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, ytphys);

	if (mpd)
		printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));
	else if (ma->mii_id1 == 0x0000 && ma->mii_id2 == 0x010a)
		printf(": YT8511 10/100/1000 PHY\n");
	else
		printf(": YT8521 10/100/1000 PHY\n");

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &ytphy_funcs;
	sc->mii_oui = MII_OUI(ma->mii_id1, ma->mii_id2);
	sc->mii_model = MII_MODEL(ma->mii_id2);
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;

	if (ma->mii_id1 == 0x0000 && ma->mii_id2 == 0x010a)
		ytphy_yt8511_init(sc);
	else
		ytphy_yt8521_init(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) ||
	    (sc->mii_extcapabilities & EXTSR_MEDIAMASK))
		mii_phy_add_media(sc);

	PHY_RESET(sc);
}

int
ytphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	if ((sc->mii_dev.dv_flags & DVF_ACTIVE) == 0)
		return (ENXIO);

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;

	case MII_DOWN:
		mii_phy_down(sc);
		return (0);
	}

	/* Update the media status. */
	mii_phy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_model != MII_MODEL_MOTORCOMM_YT8511)
		ytphy_yt8521_update(sc);
	mii_phy_update(sc, cmd);
	return (0);
}

void
ytphy_yt8511_init(struct mii_softc *sc)
{
	uint16_t tx_clk_delay_sel;
	uint16_t rx_clk_delay_en;
	uint16_t txc_delay_sel_fe;
	uint16_t addr, data;

	if (sc->mii_flags & MIIF_RXID)
		rx_clk_delay_en = YT8511_RX_CLK_DELAY_EN;
	else
		rx_clk_delay_en = 0;

	if (sc->mii_flags & MIIF_TXID) {
		tx_clk_delay_sel = YT8511_TX_CLK_DELAY_SEL_EN;
		txc_delay_sel_fe = YT8511_TXC_DELAY_SEL_FE_EN;
	} else {
		tx_clk_delay_sel = YT8511_TX_CLK_DELAY_SEL_DIS;
		txc_delay_sel_fe = YT8511_TXC_DELAY_SEL_FE_DIS;
	}

	/* Save address register. */
	addr = PHY_READ(sc, YT8511_REG_ADDR);

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8511_EXT_CLK_GATE);
	data = PHY_READ(sc, YT8511_REG_DATA);
	data &= ~YT8511_TX_CLK_DELAY_SEL_MASK;
	data &= ~YT8511_RX_CLK_DELAY_EN;
	data &= ~YT8511_CLK_25M_SEL_MASK;
	data |= tx_clk_delay_sel;
	data |= rx_clk_delay_en;
	data |= YT8511_CLK_25M_SEL_125M;
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8511_EXT_DELAY_DRIVE);
	data = PHY_READ(sc, YT8511_REG_DATA);
	data &= ~YT8511_TXC_DELAY_SEL_FE_MASK;
	data |= txc_delay_sel_fe;
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8511_EXT_SLEEP_CTRL);
	data = PHY_READ(sc, YT8511_REG_DATA);
	data |= YT8511_PLL_ON_IN_SLEEP;
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	/* Restore address register. */
	PHY_WRITE(sc, YT8511_REG_ADDR, addr);
}

void
ytphy_yt8521_init(struct mii_softc *sc)
{
	uint32_t rx_delay = 1950;
	uint32_t tx_delay = 1950;
	int rx_delay_en = 0;
	uint16_t addr, data;

#ifdef __HAVE_FDT
	if (sc->mii_pdata->mii_node) {
		rx_delay = OF_getpropint(sc->mii_pdata->mii_node,
		    "rx-internal-delay-ps", rx_delay);
		tx_delay = OF_getpropint(sc->mii_pdata->mii_node,
		    "tx-internal-delay-ps", tx_delay);
	}
#endif

	/* Save address register. */
	addr = PHY_READ(sc, YT8511_REG_ADDR);

	if ((sc->mii_flags & MIIF_RXID) == 0)
		rx_delay = 0;
	if ((sc->mii_flags & MIIF_TXID) == 0)
		tx_delay = 0;

	if (rx_delay >= 1900 && ((rx_delay - 1900) % 150) == 0) {
		rx_delay -= 1900;
		rx_delay_en = 1;
	}

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8521_EXT_CHIP_CONFIG);
	data = PHY_READ(sc, YT8511_REG_DATA);
	if (rx_delay_en)
		data |= YT8521_RXC_DLY_EN;
	else
		data &= ~YT8521_RXC_DLY_EN;
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8521_EXT_RGMII_CONFIG1);
	data = PHY_READ(sc, YT8511_REG_DATA);
	data &= ~YT8521_RX_DELAY_SEL_MASK;
	data |= (((rx_delay + 75) / 150) << YT8521_RX_DELAY_SEL_SHIFT);
	data &= ~YT8521_TX_DELAY_SEL_MASK;
	data |= (((tx_delay + 75) / 150) << YT8521_TX_DELAY_SEL_SHIFT);
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	/* Restore address register. */
	PHY_WRITE(sc, YT8511_REG_ADDR, addr);
}

void
ytphy_yt8521_update(struct mii_softc *sc)
{
#ifdef __HAVE_FDT
	struct mii_data *mii = sc->mii_pdata;
	int tx_clk_adj_en;
	int tx_clk_inv = 0;
	uint16_t addr, data;

	if (sc->mii_media_active == mii->mii_media_active)
		return;

	tx_clk_adj_en = OF_getpropbool(mii->mii_node,
	    "motorcomm,tx-clk-adj-enabled");
	if (!tx_clk_adj_en)
		return;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		tx_clk_inv = OF_getpropbool(mii->mii_node,
		    "motorcomm,tx-clk-1000-inverted");
		break;
	case IFM_100_TX:
		tx_clk_inv = OF_getpropbool(mii->mii_node,
		    "motorcomm,tx-clk-100-inverted");
		break;
	case IFM_10_T:
		tx_clk_inv = OF_getpropbool(mii->mii_node,
		    "motorcomm,tx-clk-10-inverted");
		break;
	}

	/* Save address register. */
	addr = PHY_READ(sc, YT8511_REG_ADDR);

	PHY_WRITE(sc, YT8511_REG_ADDR, YT8521_EXT_RGMII_CONFIG1);
	data = PHY_READ(sc, YT8511_REG_DATA);
	if (tx_clk_inv)
		data |= YT8521_TX_CLK_SEL;
	else
		data &= ~YT8521_TX_CLK_SEL;
	PHY_WRITE(sc, YT8511_REG_DATA, data);

	/* Restore address register. */
	PHY_WRITE(sc, YT8511_REG_ADDR, addr);
#endif
}
