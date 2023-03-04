/*	$OpenBSD: ytphy.c,v 1.2 2023/03/04 22:40:37 kettenis Exp $	*/
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

int	ytphy_match(struct device *, void *, void *);
void	ytphy_attach(struct device *, struct device *, void *);

const struct cfattach ytphy_ca = {
	sizeof(struct mii_softc), ytphy_match, ytphy_attach, mii_phy_detach
};

struct cfdriver ytphy_cd = {
	NULL, "ytphy", DV_DULL
};

int	ytphy_service(struct mii_softc *, struct mii_data *, int);

const struct mii_phy_funcs ytphy_funcs = {
	ytphy_service, ukphy_status, mii_phy_reset,
};

int
ytphy_match(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;
	
	/*
	 * The MotorComm YT8511 has a bogus MII OUIs, so match the
	 * complete ID including the rev.
	 */
	if (ma->mii_id1 == 0x0000 && ma->mii_id2 == 0x010a)
		return (10);

	return (0);
}

void
ytphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	uint16_t tx_clk_delay_sel;
	uint16_t rx_clk_delay_en;
	uint16_t txc_delay_sel_fe;
	uint16_t addr, data;

	printf(": YT8511 10/100/1000 PHY\n");

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &ytphy_funcs;
	sc->mii_oui = MII_OUI(ma->mii_id1, ma->mii_id2);
	sc->mii_model = MII_MODEL(ma->mii_id2);
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;

	if (ma->mii_flags & MIIF_RXID)
		rx_clk_delay_en = YT8511_RX_CLK_DELAY_EN;
	else
		rx_clk_delay_en = 0;

	if (ma->mii_flags & MIIF_TXID) {
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
	mii_phy_update(sc, cmd);
	return (0);
}
