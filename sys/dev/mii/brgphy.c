/*	$OpenBSD: brgphy.c,v 1.22 2005/02/05 22:30:52 brad Exp $	*/

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

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/pci/pcivar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/brgphyreg.h>

#include <dev/pci/if_bgereg.h>

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
void	brgphy_status(struct mii_softc *);
int	brgphy_mii_phy_auto(struct mii_softc *);
void	brgphy_loop(struct mii_softc *);
void	brgphy_reset(struct mii_softc *);
void	brgphy_load_dspcode(struct mii_softc *);

const struct mii_phy_funcs brgphy_funcs = {            
	brgphy_service, brgphy_status, brgphy_reset,          
};

static const struct mii_phydesc brgphys[] = {
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5400,
	  MII_STR_xxBROADCOM_BCM5400 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5401,
	  MII_STR_xxBROADCOM_BCM5401 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5411,
	  MII_STR_xxBROADCOM_BCM5411 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5421S,
	  MII_STR_xxBROADCOM_BCM5421S },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5701,
	  MII_STR_xxBROADCOM_BCM5701 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5703,
	  MII_STR_xxBROADCOM_BCM5703 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5704,
	  MII_STR_xxBROADCOM_BCM5704 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5705,
	  MII_STR_xxBROADCOM_BCM5705 },
	{ MII_OUI_xxBROADCOM,		MII_MODEL_xxBROADCOM_BCM5750,
	  MII_STR_xxBROADCOM_BCM5750 },

	{ 0,				0,
	  NULL },
};

int
brgphy_probe(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, brgphys) != NULL)
		return(10);

	return(0);
}

void
brgphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, brgphys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &brgphy_funcs;
	sc->mii_model = MII_MODEL(ma->mii_id2);
	sc->mii_rev = MII_REV(ma->mii_id2);
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags | MIIF_NOISOLATE;
	sc->mii_ticks = 0; /* XXX Should be zero. Should 0 in brgphy_reset?*/
	sc->mii_anegticks = 5;

	PHY_RESET(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) ||
	    (sc->mii_extcapabilities & EXTSR_MEDIAMASK))
		mii_phy_add_media(sc);
}

int
brgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg, speed, gig;

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

		if (sc->mii_model != MII_MODEL_xxBROADCOM_BCM5701)
			PHY_RESET(sc); /* XXX hardware bug work-around */

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, BRGPHY_MII_BMCR) & BRGPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void) brgphy_mii_phy_auto(sc);
			break;
		case IFM_1000_T:
			speed = BRGPHY_S1000;
			goto setit;
		case IFM_100_T4:
			speed = BRGPHY_S100;
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
			PHY_WRITE(sc, BRGPHY_MII_BMCR, speed);
			PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);

			if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)
				break;

			PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
			PHY_WRITE(sc, BRGPHY_MII_BMCR,
			    speed|BRGPHY_BMCR_AUTOEN|BRGPHY_BMCR_STARTNEG);

			if (sc->mii_model != MII_MODEL_xxBROADCOM_BCM5701)
 				break;
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
		reg = PHY_READ(sc, BRGPHY_MII_AUXSTS);
		if (reg & BRGPHY_AUXSTS_LINK)
			break;

		/*
		 * Only retry autonegotiation every mii_anegticks seconds.
		 */
		if (++sc->mii_ticks < sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		brgphy_mii_phy_auto(sc);
		return (0);
	}

	/* Update the media status. */
	brgphy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke the DSP on
	 * the Broadcom PHYs if the media changes.
	 */
	if (sc->mii_media_active != mii->mii_media_active || 
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		mii_phy_update(sc, cmd);
		switch (sc->mii_model) {
		case MII_MODEL_BROADCOM_BCM5400:
		case MII_MODEL_xxBROADCOM_BCM5401:
		case MII_MODEL_xxBROADCOM_BCM5411:
			brgphy_load_dspcode(sc);
			break;
		}
	}

	return (0);
}

void
brgphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);
	if (PHY_READ(sc, BRGPHY_MII_AUXSTS) & BRGPHY_AUXSTS_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);

	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		switch (PHY_READ(sc, BRGPHY_MII_AUXSTS) &
			BRGPHY_AUXSTS_AN_RES) {
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
			mii->mii_media_active |= IFM_100_T4;
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
			mii->mii_media_active |= IFM_NONE;
			break;
		}
		return;
	}

	mii->mii_media_active = ife->ifm_media;
}


int
brgphy_mii_phy_auto(struct mii_softc *sc)
{
	int ktcr = 0;

	brgphy_loop(sc);
	/* XXX need 'PHY_RESET(sc);'? Was done before getting here ... */
	ktcr = BRGPHY_1000CTL_AFD|BRGPHY_1000CTL_AHD;
	if (sc->mii_model == MII_MODEL_xxBROADCOM_BCM5701)
		ktcr |= BRGPHY_1000CTL_MSE|BRGPHY_1000CTL_MSC;
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, ktcr);
	ktcr = PHY_READ(sc, BRGPHY_MII_1000CTL);
	DELAY(1000);
	PHY_WRITE(sc, BRGPHY_MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA);
	DELAY(1000);
	PHY_WRITE(sc, BRGPHY_MII_BMCR,
	    BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	PHY_WRITE(sc, BRGPHY_MII_IMR, 0xFF00);

	return (EJUSTRETURN);
}

void
brgphy_loop(struct mii_softc *sc)
{
	u_int32_t bmsr;
	int i;

	PHY_WRITE(sc, BRGPHY_MII_BMCR, BRGPHY_BMCR_LOOP);
	for (i = 0; i < 15000; i++) {
		bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);
		if (!(bmsr & BRGPHY_BMSR_LINK)) {
#if 0
			device_printf(sc->mii_dev, "looped %d\n", i);
#endif
			break;
		}
		DELAY(10);
	}
}

void
brgphy_reset(struct mii_softc *sc)
{
	struct bge_softc *bge_sc;
	struct ifnet *ifp;
	u_int32_t val;

	mii_phy_reset(sc);

	brgphy_load_dspcode(sc);

	ifp = sc->mii_pdata->mii_ifp;
	bge_sc = ifp->if_softc;

	/*
	 * Don't enable Ethernet@WireSpeed for the 5700 or the
	 * 5705 A1 and A2 chips. Make sure we only do this test
	 * on "bge" NICs, since other drivers may use this same
	 * PHY subdriver.
	 */
	if (strncmp(ifp->if_xname, "bge", 3) == 0 &&
	    (BGE_ASICREV(bge_sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    bge_sc->bge_chipid == BGE_CHIPID_BCM5705_A1 ||
	    bge_sc->bge_chipid == BGE_CHIPID_BCM5705_A2))
		return;
 
	/* Enable Ethernet@WireSpeed. */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7007);
	val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, val | (1 << 15) | (1 << 4));

	/* Enable Link LED on Dell boxes */
	if (bge_sc->bge_no_3_led) {
		PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL, 
		    PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL)
		    & ~BRGPHY_PHY_EXTCTL_3_LED);
	}
}

struct bcm_dspcode {
	int		reg;
	u_int16_t	val;
};

static const struct bcm_dspcode bcm5401_dspcode[] = {
	{ BRGPHY_MII_AUXCTL,            0x0c20 },
	{ BRGPHY_MII_DSP_ADDR_REG,      0x0012 },
	{ BRGPHY_MII_DSP_RW_PORT,       0x1804 },
	{ BRGPHY_MII_DSP_ADDR_REG,      0x0013 },
	{ BRGPHY_MII_DSP_RW_PORT,       0x1204 },
	{ BRGPHY_MII_DSP_ADDR_REG,      0x8006 },
	{ BRGPHY_MII_DSP_RW_PORT,       0x0132 },
	{ BRGPHY_MII_DSP_ADDR_REG,      0x8006 },
	{ BRGPHY_MII_DSP_RW_PORT,       0x0232 },
	{ BRGPHY_MII_DSP_ADDR_REG,      0x201f },
	{ BRGPHY_MII_DSP_RW_PORT,       0x0a20 },
	{ 0,                            0 },
};

static const struct bcm_dspcode bcm5411_dspcode[] = {
	{ 0x1c,                         0x8c23 },
	{ 0x1c,                         0x8ca3 },
	{ 0x1c,                         0x8c23 },
	{ 0,                            0 },
};

static const struct bcm_dspcode bcm5703_dspcode[] = {
	{ BRGPHY_MII_AUXCTL,		0x0c00 },
	{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
	{ BRGPHY_MII_DSP_RW_PORT,	0x2aaa },
	{ 0,				0 },
};

static const struct bcm_dspcode bcm5704_dspcode[] = {
	{ 0x1c,				0x8d68 },
	{ 0x1c,				0x8d68 },
	{ 0,				0 },
};

static const struct bcm_dspcode bcm5750_dspcode[] = {
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

void
brgphy_load_dspcode(struct mii_softc *sc)
{
	const struct bcm_dspcode *dsp = NULL;
	int wait=0, i;

	switch (sc->mii_model) {
	case MII_MODEL_BROADCOM_BCM5400:
		dsp = bcm5401_dspcode;
		wait=40;
		break;
	case MII_MODEL_BROADCOM_BCM5401:
		if (sc->mii_rev == 1 || sc->mii_rev == 3) {
			dsp = bcm5401_dspcode;
			wait=40;
		}
		break;
	case MII_MODEL_BROADCOM_BCM5411:
		dsp = bcm5411_dspcode;
		break;
	case MII_MODEL_xxBROADCOM_BCM5703:
		dsp = bcm5703_dspcode;
		break;
	case MII_MODEL_xxBROADCOM_BCM5704:
		dsp = bcm5704_dspcode;
		break;
	case MII_MODEL_xxBROADCOM_BCM5750:
		dsp = bcm5750_dspcode;
		break;
	}

	if (dsp == NULL)
		return;

	for (i = 0; dsp[i].reg != 0; i++)
		PHY_WRITE(sc, dsp[i].reg, dsp[i].val);

	if (wait > 0)
		DELAY(wait);
}
