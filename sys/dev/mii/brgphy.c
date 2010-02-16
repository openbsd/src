/*	$OpenBSD: brgphy.c,v 1.90 2010/02/16 09:12:33 sthen Exp $	*/

/*
 * Copyright (c) 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: brgphy.c,v 1.8 2002/03/22 06:38:52 wpaul Exp $
 */

/*
 * Driver for the Broadcom BCR5400 1000baseTX PHY. Speed is always
 * 1000mbps; all we need to negotiate here is full or half duplex.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/pci/pcivar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/brgphyreg.h>

#include <dev/pci/if_bgereg.h>
#include <dev/pci/if_bnxreg.h>

int brgphy_probe(struct device *, void *, void *);
void brgphy_attach(struct device *, struct device *, void *);

struct cfattach brgphy_ca = {
	sizeof(struct mii_softc), brgphy_probe, brgphy_attach, mii_phy_detach,
	    mii_phy_activate
};

struct cfdriver brgphy_cd = {
	NULL, "brgphy", DV_DULL
};

int	brgphy_service(struct mii_softc *, struct mii_data *, int);
void	brgphy_copper_status(struct mii_softc *);
void	brgphy_fiber_status(struct mii_softc *);
void	brgphy_5708s_status(struct mii_softc *);
int	brgphy_mii_phy_auto(struct mii_softc *);
void	brgphy_loop(struct mii_softc *);
void	brgphy_reset(struct mii_softc *);
void	brgphy_bcm5401_dspcode(struct mii_softc *);
void	brgphy_bcm5411_dspcode(struct mii_softc *);
void	brgphy_bcm5421_dspcode(struct mii_softc *);
void	brgphy_bcm54k2_dspcode(struct mii_softc *);
void	brgphy_adc_bug(struct mii_softc *);
void	brgphy_5704_a0_bug(struct mii_softc *);
void	brgphy_ber_bug(struct mii_softc *);
void	brgphy_crc_bug(struct mii_softc *);
void	brgphy_disable_early_dac(struct mii_softc *sc);
void	brgphy_jumbo_settings(struct mii_softc *);
void	brgphy_eth_wirespeed(struct mii_softc *);

const struct mii_phy_funcs brgphy_copper_funcs = {            
	brgphy_service, brgphy_copper_status, brgphy_reset,          
};

const struct mii_phy_funcs brgphy_fiber_funcs = {
	brgphy_service, brgphy_fiber_status, brgphy_reset,
};

const struct mii_phy_funcs brgphy_5708s_funcs = {
	brgphy_service, brgphy_5708s_status, brgphy_reset,
};

static const struct mii_phydesc brgphys[] = {
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5400,
	  MII_STR_xxBROADCOM_BCM5400 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5401,
	  MII_STR_xxBROADCOM_BCM5401 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5411,
	  MII_STR_xxBROADCOM_BCM5411 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5421,
	  MII_STR_xxBROADCOM_BCM5421 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM54K2,
	  MII_STR_xxBROADCOM_BCM54K2 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5461,
	  MII_STR_xxBROADCOM_BCM5461 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5462,
	  MII_STR_xxBROADCOM_BCM5462 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5464,
	  MII_STR_xxBROADCOM_BCM5464 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5701,
	  MII_STR_xxBROADCOM_BCM5701 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5703,
	  MII_STR_xxBROADCOM_BCM5703 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5704,
	  MII_STR_xxBROADCOM_BCM5704 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5705,
	  MII_STR_xxBROADCOM_BCM5705 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5714,
	  MII_STR_xxBROADCOM_BCM5714 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5750,
	  MII_STR_xxBROADCOM_BCM5750 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5752,
	  MII_STR_xxBROADCOM_BCM5752 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5780,
	  MII_STR_xxBROADCOM_BCM5780 },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM54XX,
	  MII_STR_xxBROADCOM2_BCM54XX },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5722,
	  MII_STR_xxBROADCOM2_BCM5722 },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5755,
	  MII_STR_xxBROADCOM2_BCM5755 },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5761,
	  MII_STR_xxBROADCOM2_BCM5761 },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5784,
	  MII_STR_xxBROADCOM2_BCM5784 },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5787,
	  MII_STR_xxBROADCOM2_BCM5787 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5706,
	  MII_STR_xxBROADCOM_BCM5706 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5708C,
	  MII_STR_xxBROADCOM_BCM5708C },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5708S,
	  MII_STR_xxBROADCOM2_BCM5708S },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5709C,
	  MII_STR_xxBROADCOM2_BCM5709C },
	{ MII_OUI_xxBROADCOM2,		MII_MODEL_xxBROADCOM2_BCM5709CAX,
	  MII_STR_xxBROADCOM2_BCM5709CAX },
	{ MII_OUI_xxBROADCOM3,		MII_MODEL_xxBROADCOM3_BCM57780,
	  MII_STR_xxBROADCOM3_BCM57780 },
	{ MII_OUI_BROADCOM2,		MII_MODEL_BROADCOM2_BCM5906,
	  MII_STR_BROADCOM2_BCM5906 },

	{ 0,				0,
	  NULL },
};

int
brgphy_probe(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, brgphys) != NULL)
		return (10);

	return (0);
}

void
brgphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct bnx_softc *bnx_sc = NULL;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;
	char *devname;

	devname = sc->mii_dev.dv_parent->dv_cfdata->cf_driver->cd_name;

	mpd = mii_phy_match(ma, brgphys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_model = MII_MODEL(ma->mii_id2);
	sc->mii_rev = MII_REV(ma->mii_id2);
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	if (sc->mii_flags & MIIF_HAVEFIBER) {
		if (MII_OUI(ma->mii_id1, ma->mii_id2) ==
		    MII_OUI_xxBROADCOM &&
		    (sc->mii_model == MII_MODEL_xxBROADCOM_BCM5706 ||
		     sc->mii_model == MII_MODEL_xxBROADCOM_BCM5714 ||
		     sc->mii_model == MII_MODEL_xxBROADCOM_BCM5780))
			sc->mii_funcs = &brgphy_fiber_funcs;
		else if (MII_OUI(ma->mii_id1, ma->mii_id2) ==
		    MII_OUI_xxBROADCOM2 && sc->mii_model ==
		    MII_MODEL_xxBROADCOM2_BCM5708S)
			sc->mii_funcs = &brgphy_5708s_funcs;
	} else
		sc->mii_funcs = &brgphy_copper_funcs;
	sc->mii_anegticks = MII_ANEGTICKS;

	sc->mii_flags |= MIIF_NOISOLATE | MIIF_NOLOOP;

	if (strcmp(devname, "bnx") == 0)
		bnx_sc = sc->mii_pdata->mii_ifp->if_softc;

	PHY_RESET(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);

	mii_phy_add_media(sc);

	if (sc->mii_flags & MIIF_HAVEFIBER && bnx_sc &&
	   (bnx_sc->bnx_phy_flags & BNX_PHY_2_5G_CAPABLE_FLAG)) {
		sc->mii_anegticks = MII_ANEGTICKS_GIGE;
		ifmedia_add(&mii->mii_media,
		    IFM_MAKEWORD(IFM_ETHER, IFM_2500_SX, IFM_FDX,
		    sc->mii_inst), 0, NULL);
	}
}

int
brgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg, speed = 0, gig;

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

		PHY_RESET(sc); /* XXX hardware bug work-around */

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			(void) brgphy_mii_phy_auto(sc);
			break;
		case IFM_2500_SX:
			speed = BRGPHY_5708S_BMCR_2500;
			goto setit;
		case IFM_1000_T:
			speed = BRGPHY_S1000;
			goto setit;
		case IFM_100_TX:
			speed = BRGPHY_S100;
			goto setit;
		case IFM_10_T:
			speed = BRGPHY_S10;
setit:
			brgphy_loop(sc);
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				speed |= BRGPHY_BMCR_FDX;
				gig = BRGPHY_1000CTL_AFD;
			} else {
				gig = BRGPHY_1000CTL_AHD;
			}

			PHY_WRITE(sc, BRGPHY_MII_1000CTL, 0);
			PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);
			PHY_WRITE(sc, BRGPHY_MII_BMCR, speed);

			if ((IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T) &&
			    (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_SX) &&
			    (IFM_SUBTYPE(ife->ifm_media) != IFM_2500_SX))
				break;

			PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
			PHY_WRITE(sc, BRGPHY_MII_BMCR,
			    speed|BRGPHY_BMCR_AUTOEN|BRGPHY_BMCR_STARTNEG);

			if (sc->mii_model != MII_MODEL_xxBROADCOM_BCM5701)
 				break;

			if (mii->mii_media.ifm_media & IFM_ETH_MASTER)
				gig |= BRGPHY_1000CTL_MSE|BRGPHY_1000CTL_MSC;
			PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
			break;
		default:
			return (EINVAL);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			break;

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			break;

		/*
		 * Only retry autonegotiation every mii_anegticks seconds.
		 */
		if (++sc->mii_ticks <= sc->mii_anegticks)
			break;

		sc->mii_ticks = 0;
		brgphy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	mii_phy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke the DSP on
	 * the Broadcom PHYs if the media changes.
	 */
	if (sc->mii_media_active != mii->mii_media_active || 
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		switch (sc->mii_model) {
		case MII_MODEL_BROADCOM_BCM5400:
			brgphy_bcm5401_dspcode(sc);
			break;
		case MII_MODEL_xxBROADCOM_BCM5401:
			if (sc->mii_rev == 1 || sc->mii_rev == 3)
				brgphy_bcm5401_dspcode(sc);
			break;
		case MII_MODEL_xxBROADCOM_BCM5411:
			brgphy_bcm5411_dspcode(sc);
			break;
		}
	}

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);

	return (0);
}

void
brgphy_copper_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR) | PHY_READ(sc, BRGPHY_MII_BMSR);
	if (bmsr & BRGPHY_BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);
	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		int auxsts;

		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		auxsts = PHY_READ(sc, BRGPHY_MII_AUXSTS);

		switch (auxsts & BRGPHY_AUXSTS_AN_RES) {
		case BRGPHY_RES_1000FD:
			mii->mii_media_active |= IFM_1000_T | IFM_FDX;
			break;
		case BRGPHY_RES_1000HD:
			mii->mii_media_active |= IFM_1000_T | IFM_HDX;
			break;
		case BRGPHY_RES_100FD:
			mii->mii_media_active |= IFM_100_TX | IFM_FDX;
			break;
		case BRGPHY_RES_100T4:
			mii->mii_media_active |= IFM_100_T4 | IFM_HDX;
			break;
		case BRGPHY_RES_100HD:
			mii->mii_media_active |= IFM_100_TX | IFM_HDX;
			break;
		case BRGPHY_RES_10FD:
			mii->mii_media_active |= IFM_10_T | IFM_FDX;
			break;
		case BRGPHY_RES_10HD:
			mii->mii_media_active |= IFM_10_T | IFM_HDX;
			break;
		default:
			if (sc->mii_model == MII_MODEL_BROADCOM2_BCM5906) {
				mii->mii_media_active |= (auxsts &
				    BRGPHY_RES_100) ? IFM_100_TX : IFM_10_T;
				mii->mii_media_active |= (auxsts &
				    BRGPHY_RES_FULL) ? IFM_FDX : IFM_HDX;
				break;
			}
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if (mii->mii_media_active & IFM_FDX)
			mii->mii_media_active |= mii_phy_flowstatus(sc);

		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) {
			if (PHY_READ(sc, BRGPHY_MII_1000STS) &
			    BRGPHY_1000STS_MSR)
				mii->mii_media_active |= IFM_ETH_MASTER;
		}
	} else
		mii->mii_media_active = ife->ifm_media;
}

void
brgphy_fiber_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR) | PHY_READ(sc, BRGPHY_MII_BMSR);
	if (bmsr & BRGPHY_BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);
	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		int val;

		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		mii->mii_media_active |= IFM_1000_SX;

		val = PHY_READ(sc, BRGPHY_SERDES_ANAR) &
		      PHY_READ(sc, BRGPHY_SERDES_ANLPAR);

		if (val & BRGPHY_SERDES_ANAR_FDX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;

		if (mii->mii_media_active & IFM_FDX)
			mii->mii_media_active |= mii_phy_flowstatus(sc);
	} else
		mii->mii_media_active = ife->ifm_media;
}

void
brgphy_5708s_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR) | PHY_READ(sc, BRGPHY_MII_BMSR);
	if (bmsr & BRGPHY_BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);
	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		int xstat;

		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR,
		    BRGPHY_5708S_DIG_PG0);

		xstat = PHY_READ(sc, BRGPHY_5708S_PG0_1000X_STAT1);

		switch (xstat & BRGPHY_5708S_PG0_1000X_STAT1_SPEED_MASK) {
		case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_10:
			mii->mii_media_active |= IFM_10_FL;
			break;
		case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_100:
			mii->mii_media_active |= IFM_100_FX;
			break;
		case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_1G:
			mii->mii_media_active |= IFM_1000_SX;
			break;
		case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_25G:
			mii->mii_media_active |= IFM_2500_SX;
			break;
		}

		if (xstat & BRGPHY_5708S_PG0_1000X_STAT1_FDX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

int
brgphy_mii_phy_auto(struct mii_softc *sc)
{
	int anar, ktcr = 0;

	PHY_RESET(sc);

	if (sc->mii_flags & MIIF_HAVEFIBER) {
		anar = BRGPHY_SERDES_ANAR_FDX | BRGPHY_SERDES_ANAR_HDX;
		if (sc->mii_flags & MIIF_DOPAUSE)
			anar |= BRGPHY_SERDES_ANAR_BOTH_PAUSE;
		PHY_WRITE(sc, BRGPHY_SERDES_ANAR, anar);
	} else {
		anar = BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA;
		if (sc->mii_flags & MIIF_DOPAUSE)
			anar |= BRGPHY_ANAR_ASP | BRGPHY_ANAR_PC;
		PHY_WRITE(sc, BRGPHY_MII_ANAR, anar);
	}

	/* Enable speed in the 1000baseT control register */
	ktcr = BRGPHY_1000CTL_AFD | BRGPHY_1000CTL_AHD;
	if (sc->mii_model == MII_MODEL_xxBROADCOM_BCM5701)
		ktcr |= BRGPHY_1000CTL_MSE | BRGPHY_1000CTL_MSC;
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, ktcr);
	ktcr = PHY_READ(sc, BRGPHY_MII_1000CTL);

	/* Start autonegotiation */
	PHY_WRITE(sc, BRGPHY_MII_BMCR,
	    BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	PHY_WRITE(sc, BRGPHY_MII_IMR, 0xFF00);

	return (EJUSTRETURN);
}

/* Enable loopback to force the link down. */
void
brgphy_loop(struct mii_softc *sc)
{
	u_int32_t bmsr;
	int i;

	PHY_WRITE(sc, BRGPHY_MII_BMCR, BRGPHY_BMCR_LOOP);
	for (i = 0; i < 15000; i++) {
		bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);
		if (!(bmsr & BRGPHY_BMSR_LINK))
			break;
		DELAY(10);
	}
}

void
brgphy_reset(struct mii_softc *sc)
{
	struct bge_softc *bge_sc = NULL;
	struct bnx_softc *bnx_sc = NULL;
	char *devname;

	devname = sc->mii_dev.dv_parent->dv_cfdata->cf_driver->cd_name;

	mii_phy_reset(sc);

	switch (sc->mii_model) {
	case MII_MODEL_BROADCOM_BCM5400:
		brgphy_bcm5401_dspcode(sc);
		break;
	case MII_MODEL_BROADCOM_BCM5401:
		if (sc->mii_rev == 1 || sc->mii_rev == 3)
			brgphy_bcm5401_dspcode(sc);
		break;
	case MII_MODEL_BROADCOM_BCM5411:
		brgphy_bcm5411_dspcode(sc);
		break;
	case MII_MODEL_xxBROADCOM_BCM5421:
		brgphy_bcm5421_dspcode(sc);
		break;
	case MII_MODEL_xxBROADCOM_BCM54K2:
		brgphy_bcm54k2_dspcode(sc);
		break;
	}

	/* Handle any bge (NetXtreme/NetLink) workarounds. */
	if (strcmp(devname, "bge") == 0) {
		if (!(sc->mii_flags & MIIF_HAVEFIBER)) {
			bge_sc = sc->mii_pdata->mii_ifp->if_softc;

			if (bge_sc->bge_flags & BGE_PHY_ADC_BUG)
				brgphy_adc_bug(sc);
			if (bge_sc->bge_flags & BGE_PHY_5704_A0_BUG)
				brgphy_5704_a0_bug(sc);
			if (bge_sc->bge_flags & BGE_PHY_BER_BUG)
				brgphy_ber_bug(sc);
			else if (bge_sc->bge_flags & BGE_PHY_JITTER_BUG) {
				PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x0c00);
				PHY_WRITE(sc, BRGPHY_MII_DSP_ADDR_REG,
				    0x000a);

				if (bge_sc->bge_flags & BGE_PHY_ADJUST_TRIM) {
					PHY_WRITE(sc, BRGPHY_MII_DSP_RW_PORT,
					    0x110b);
					PHY_WRITE(sc, BRGPHY_TEST1,
					    BRGPHY_TEST1_TRIM_EN | 0x4);
				} else {
					PHY_WRITE(sc, BRGPHY_MII_DSP_RW_PORT,
					    0x010b);
				}

				PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x0400);
			}
			if (bge_sc->bge_flags & BGE_PHY_CRC_BUG)
				brgphy_crc_bug(sc);

			/* Set Jumbo frame settings in the PHY. */
			if (bge_sc->bge_flags & BGE_JUMBO_CAPABLE)
				brgphy_jumbo_settings(sc);

			/* Adjust output voltage */
			if (sc->mii_model == MII_MODEL_BROADCOM2_BCM5906)
				PHY_WRITE(sc, BRGPHY_MII_EPHY_PTEST, 0x12);

			/* Enable Ethernet@Wirespeed */
			if (!(bge_sc->bge_flags & BGE_NO_ETH_WIRE_SPEED))
				brgphy_eth_wirespeed(sc);

			/* Enable Link LED on Dell boxes */
			if (bge_sc->bge_flags & BGE_NO_3LED) {
				PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL, 
				PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL)
					& ~BRGPHY_PHY_EXTCTL_3_LED);
			}
		}
	/* Handle any bnx (NetXtreme II) workarounds. */
	} else if (strcmp(devname, "bnx") == 0) {
		bnx_sc = sc->mii_pdata->mii_ifp->if_softc;

		if (BNX_CHIP_NUM(bnx_sc) == BNX_CHIP_NUM_5708 &&
		    ISSET(BNX_CHIP_BOND_ID(bnx_sc),
		     BNX_CHIP_BOND_ID_SERDES_BIT)) {
			/* Store autoneg capabilities/results in digital block (Page 0) */
			PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, BRGPHY_5708S_DIG3_PG2);
			PHY_WRITE(sc, BRGPHY_5708S_PG2_DIGCTL_3_0, 
				BRGPHY_5708S_PG2_DIGCTL_3_0_USE_IEEE);
			PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, BRGPHY_5708S_DIG_PG0);

			/* Enable fiber mode and autodetection */
			PHY_WRITE(sc, BRGPHY_5708S_PG0_1000X_CTL1, 
				PHY_READ(sc, BRGPHY_5708S_PG0_1000X_CTL1) | 
				BRGPHY_5708S_PG0_1000X_CTL1_AUTODET_EN | 
				BRGPHY_5708S_PG0_1000X_CTL1_FIBER_MODE);

			/* Enable parallel detection */
			PHY_WRITE(sc, BRGPHY_5708S_PG0_1000X_CTL2, 
				PHY_READ(sc, BRGPHY_5708S_PG0_1000X_CTL2) | 
				BRGPHY_5708S_PG0_1000X_CTL2_PAR_DET_EN);

			/* Advertise 2.5G support through next page during autoneg */
			if (bnx_sc->bnx_phy_flags & BNX_PHY_2_5G_CAPABLE_FLAG)
				PHY_WRITE(sc, BRGPHY_5708S_ANEG_NXT_PG_XMIT1, 
					PHY_READ(sc, BRGPHY_5708S_ANEG_NXT_PG_XMIT1) | 
					BRGPHY_5708S_ANEG_NXT_PG_XMIT1_25G);

			/* Increase TX signal amplitude */
			if ((BNX_CHIP_ID(bnx_sc) == BNX_CHIP_ID_5708_A0) ||
			    (BNX_CHIP_ID(bnx_sc) == BNX_CHIP_ID_5708_B0) ||
			    (BNX_CHIP_ID(bnx_sc) == BNX_CHIP_ID_5708_B1)) {
				PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
					BRGPHY_5708S_TX_MISC_PG5);
				PHY_WRITE(sc, BRGPHY_5708S_PG5_TXACTL1, 
					PHY_READ(sc, BRGPHY_5708S_PG5_TXACTL1) &
					~BRGPHY_5708S_PG5_TXACTL1_VCM);
				PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
					BRGPHY_5708S_DIG_PG0);
			}

			/* Backplanes use special driver/pre-driver/pre-emphasis values. */
			if ((bnx_sc->bnx_shared_hw_cfg & BNX_SHARED_HW_CFG_PHY_BACKPLANE) &&
			    (bnx_sc->bnx_port_hw_cfg & BNX_PORT_HW_CFG_CFG_TXCTL3_MASK)) {
					PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
						BRGPHY_5708S_TX_MISC_PG5);
					PHY_WRITE(sc, BRGPHY_5708S_PG5_TXACTL3, 
						bnx_sc->bnx_port_hw_cfg & 
						BNX_PORT_HW_CFG_CFG_TXCTL3_MASK);
					PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR,
						BRGPHY_5708S_DIG_PG0);
			}
		} else if (BNX_CHIP_NUM(bnx_sc) == BNX_CHIP_NUM_5709) {
			if (BNX_CHIP_REV(bnx_sc) == BNX_CHIP_REV_Ax ||
			    BNX_CHIP_REV(bnx_sc) == BNX_CHIP_REV_Bx)
				brgphy_disable_early_dac(sc);

			/* Set Jumbo frame settings in the PHY. */
			brgphy_jumbo_settings(sc);

			/* Enable Ethernet@Wirespeed */
			brgphy_eth_wirespeed(sc);
		} else {
			if (!(sc->mii_flags & MIIF_HAVEFIBER)) {
				brgphy_ber_bug(sc);

				/* Set Jumbo frame settings in the PHY. */
				brgphy_jumbo_settings(sc);

				/* Enable Ethernet@Wirespeed */
				brgphy_eth_wirespeed(sc);
			}
		}
	}
}

/* Disable tap power management */
void
brgphy_bcm5401_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c20 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x0012 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x1804 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x0013 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x1204 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x8006 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0132 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x8006 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0232 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0a20 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
	DELAY(40);
}

/* Setting some undocumented voltage */
void
brgphy_bcm5411_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ 0x1c,				0x8c23 },
		{ 0x1c,				0x8ca3 },
		{ 0x1c,				0x8c23 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

void
brgphy_bcm5421_dspcode(struct mii_softc *sc)
{
	uint16_t data;

	/* Set Class A mode */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x1007);
	data = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, data | 0x0400);

	/* Set FFE gamma override to -0.125 */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x0007);
	data = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, data | 0x0800);
	PHY_WRITE(sc, BRGPHY_MII_DSP_ADDR_REG, 0x000a);
	data = PHY_READ(sc, BRGPHY_MII_DSP_RW_PORT);
	PHY_WRITE(sc, BRGPHY_MII_DSP_RW_PORT, data | 0x0200);
}

void
brgphy_bcm54k2_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ 4,				0x01e1 },
		{ 9,				0x0300 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

void
brgphy_adc_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x2aaa },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x000a },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0323 },
		{ BRGPHY_MII_AUXCTL,		0x0400 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

void
brgphy_5704_a0_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ 0x1c,				0x8d68 },
		{ 0x1c,				0x8d68 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

void
brgphy_ber_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x000a },
		{ BRGPHY_MII_DSP_RW_PORT,	0x310b },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x9506 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x401f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x14e2 },
		{ BRGPHY_MII_AUXCTL,		0x0400 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

/* BCM5701 A0/B0 CRC bug workaround */
void
brgphy_crc_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_DSP_ADDR_REG,	0x0a75 },
		{ 0x1c,				0x8c68 },
		{ 0x1c,				0x8d68 },
		{ 0x1c,				0x8c68 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

void
brgphy_disable_early_dac(struct mii_softc *sc)
{
	uint32_t val;

	PHY_WRITE(sc, BRGPHY_MII_DSP_ADDR_REG, 0x0f08);
	val = PHY_READ(sc, BRGPHY_MII_DSP_RW_PORT);
	val &= ~(1 << 8);
	PHY_WRITE(sc, BRGPHY_MII_DSP_RW_PORT, val);

}

void
brgphy_jumbo_settings(struct mii_softc *sc)
{
	u_int32_t val;

	/* Set Jumbo frame settings in the PHY. */
	if (sc->mii_model == MII_MODEL_BROADCOM_BCM5401) {
		/* Cannot do read-modify-write on the BCM5401 */
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x4c20);
	} else {
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7);
		val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
			val & ~(BRGPHY_AUXCTL_LONG_PKT | 0x7));
	}

	val = PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL);
	PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL,
		val & ~BRGPHY_PHY_EXTCTL_HIGH_LA);
}

void
brgphy_eth_wirespeed(struct mii_softc *sc)
{
	u_int32_t val;

	/* Enable Ethernet@Wirespeed */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7007);
	val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
		(val | (1 << 15) | (1 << 4)));
}
