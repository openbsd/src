/*	$OpenBSD: bmtphy.c,v 1.20 2015/03/14 03:38:47 jsg Exp $	*/
/*	$NetBSD: bmtphy.c,v 1.17 2005/01/17 13:17:45 scw Exp $	*/

/*-
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver for Broadcom BCM5201/BCM5202 "Mini-Theta" PHYs.  This also
 * drives the PHY on the 3Com 3c905C.  The 3c905C's PHY is described in
 * the 3c905C data sheet.
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

#include <dev/mii/bmtphyreg.h>

int	bmtphymatch(struct device *, void *, void *);
void	bmtphyattach(struct device *, struct device *, void *);

struct cfattach bmtphy_ca = {
	sizeof(struct mii_softc), bmtphymatch, bmtphyattach, mii_phy_detach
};

struct cfdriver bmtphy_cd = {
	NULL, "bmtphy", DV_DULL
};

int	bmtphy_service(struct mii_softc *, struct mii_data *, int);
void	bmtphy_status(struct mii_softc *);
void	bmtphy_reset(struct mii_softc *);

const struct mii_phy_funcs bmtphy_funcs = {
	bmtphy_service, bmtphy_status, bmtphy_reset,
};

static const struct mii_phydesc bmtphys[] = {
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_3C905B,
	  MII_STR_BROADCOM_3C905B },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_3C905C,
	  MII_STR_BROADCOM_3C905C },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM4401,
	  MII_STR_BROADCOM_BCM4401 },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM5201,
	  MII_STR_BROADCOM_BCM5201 },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM5214,
	  MII_STR_BROADCOM_BCM5214 },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM5220,
	  MII_STR_BROADCOM_BCM5220 },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM5221,
	  MII_STR_BROADCOM_BCM5221 },
	{ MII_OUI_BROADCOM,		MII_MODEL_BROADCOM_BCM5222,
	  MII_STR_BROADCOM_BCM5222 },

	{ 0,				0,
	  NULL },
};

int
bmtphymatch(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, bmtphys) != NULL)
		return (10);

	return (0);
}

void
bmtphyattach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, bmtphys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_model = MII_MODEL(ma->mii_id2);
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &bmtphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS;

	PHY_RESET(sc);

	/*
	 * XXX Check AUX_STS_FX_MODE to set MIIF_HAVE_FIBER?
	 */

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_MEDIAMASK)
		mii_phy_add_media(sc);
}

int
bmtphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

void
bmtphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, aux_csr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * The later are only valid if autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		aux_csr = PHY_READ(sc, MII_BMTPHY_AUX_CSR);
		if (aux_csr & AUX_CSR_SPEED)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;

		if (aux_csr & AUX_CSR_FDX)
			mii->mii_media_active |= mii_phy_flowstatus(sc) | IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

void
bmtphy_reset(struct mii_softc *sc)
{
	u_int16_t data;

	mii_phy_reset(sc);

	if (sc->mii_model == MII_MODEL_BROADCOM_BCM5221) {
		/* Enable shadow register mode */
		data = PHY_READ(sc, 0x1f);
		PHY_WRITE(sc, 0x1f, data | 0x0080);

		/* Enable APD (Auto PowerDetect) */
		data = PHY_READ(sc, MII_BMTPHY_AUX2);
		PHY_WRITE(sc, MII_BMTPHY_AUX2, data | 0x0020);

		/* Enable clocks across APD for
		 * Auto-MDIX functionality */
		data = PHY_READ(sc, MII_BMTPHY_INTR);
		PHY_WRITE(sc, MII_BMTPHY_INTR, data | 0x0004);

		/* Disable shadow register mode */
		data = PHY_READ(sc, 0x1f);
		PHY_WRITE(sc, 0x1f, data & ~0x0080);
	}
}
