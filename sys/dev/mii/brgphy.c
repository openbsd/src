/*	$OpenBSD: brgphy.c,v 1.14 2002/11/26 06:01:28 nate Exp $	*/

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
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/brgphyreg.h>

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

int	brgphy_mii_phy_auto(struct mii_softc *, int);
extern void	mii_phy_auto_timeout(void *);
void	brgphy_reset(struct mii_softc *);
void	brgphy_load_dspcode(struct mii_softc *);

int
brgphy_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct mii_attach_args *ma = aux;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    (MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5400 ||
	     MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5401 ||
	     MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5411 ||
	     MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5421S ||
	     MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5701 ||
	     MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5703))
		return(10);

	return(0);
}

void
brgphy_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	char *model;

	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5400 ||
	    MII_MODEL(ma->mii_id2) == MII_MODEL_BROADCOM_BCM5400)
		model = MII_STR_BROADCOM_BCM5400;
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_BROADCOM_BCM5401)
		model = MII_STR_BROADCOM_BCM5401;
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_BROADCOM_BCM5411)
		model = MII_STR_BROADCOM_BCM5411;
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5421S)
		model = MII_STR_xxBROADCOM_BCM5421S;
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5701)
		model = MII_STR_xxBROADCOM_BCM5701;
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5703)
		model = MII_STR_xxBROADCOM_BCM5703;

	printf(": %s, rev. %d\n", model, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = brgphy_service;
	sc->mii_status = brgphy_status;
	sc->mii_pdata = mii;
	sc->mii_flags |= MIIF_NOISOLATE;
	sc->mii_anegticks = 10;

	brgphy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
        if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
        if ((sc->mii_capabilities & BMSR_MEDIAMASK) ||
            (sc->mii_extcapabilities & EXTSR_MEDIAMASK))
                mii_phy_add_media(sc);
}

int
brgphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg, speed;

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

		PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL,
		    BRGPHY_PHY_EXTCTL_HIGH_LA|BRGPHY_PHY_EXTCTL_EN_LTR);
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
		    BRGPHY_AUXCTL_LONG_PKT|BRGPHY_AUXCTL_TX_TST);
		PHY_WRITE(sc, BRGPHY_MII_IMR, 0xFF00);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, BRGPHY_MII_BMCR) & BRGPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void) brgphy_mii_phy_auto(sc, 1);
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
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, BRGPHY_MII_BMCR,
				    BRGPHY_BMCR_FDX|speed);
			} else {
				PHY_WRITE(sc, BRGPHY_MII_BMCR, speed);
			}
			PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);

			if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)
				break;

			/*
			 * On IFM_1000_X only,
			 * when setting the link manually, one side must
			 * be the master and the other the slave. However
			 * ifmedia doesn't give us a good way to specify
			 * this, so we fake it by using one of the LINK
			 * flags. If LINK0 is set, we program the PHY to
			 * be a master, otherwise it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, BRGPHY_MII_1000CTL,
				    BRGPHY_1000CTL_MSE|BRGPHY_1000CTL_MSC);
			} else {
				PHY_WRITE(sc, BRGPHY_MII_1000CTL,
				    BRGPHY_1000CTL_MSE);
			}
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
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, BRGPHY_MII_AUXSTS);
		if (reg & BRGPHY_AUXSTS_LINK)
			break;

		brgphy_reset(sc);
		if (brgphy_mii_phy_auto(sc, 0) == EJUSTRETURN)
			return(0);
		break;
	}

	/* Update the media status. */
	brgphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

void
brgphy_status(sc)
	struct mii_softc *sc;
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
		}
		return;
	}

	mii->mii_media_active = ife->ifm_media;
}


int
brgphy_mii_phy_auto(mii, waitfor)
	struct mii_softc *mii;
	int waitfor;
{
	int bmsr, ktcr = 0, i;

	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		brgphy_reset(mii);
		PHY_WRITE(mii, BRGPHY_MII_BMCR, 0);
		DELAY(1000);
		ktcr = PHY_READ(mii, BRGPHY_MII_1000CTL);
		PHY_WRITE(mii, BRGPHY_MII_1000CTL, ktcr |
		    BRGPHY_1000CTL_AFD|BRGPHY_1000CTL_AHD);
		ktcr = PHY_READ(mii, BRGPHY_MII_1000CTL);
		DELAY(1000);
		PHY_WRITE(mii, BRGPHY_MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
		DELAY(1000);
		PHY_WRITE(mii, BRGPHY_MII_BMCR,
		    BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
		PHY_WRITE(mii, BRGPHY_MII_IMR, 0xFF00);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(mii, BRGPHY_MII_BMSR)) &
			    BRGPHY_BMSR_ACOMP)
				return (0);
			DELAY(1000);
#if 0
		if ((bmsr & BMSR_ACOMP) == 0)
			printf("%s: autonegotiation failed to complete\n",
			    mii->mii_dev.dv_xname);
#endif
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		mii->mii_flags |= MIIF_DOINGAUTO;
		timeout_set(&mii->mii_phy_timo, mii_phy_auto_timeout, mii);
		timeout_add(&mii->mii_phy_timo, hz >> 1);
	}
	return (EJUSTRETURN);
}

void
brgphy_reset(sc)
	struct mii_softc *sc;
{
	brgphy_load_dspcode(sc);
}

struct bcm_dspcode {
	int		reg;
	u_int16_t	val;
};

static const struct bcm_dspcode bcm5401_dspcode[] = {
	{ BRGPHY_MII_AUXCTL,            0x4c20 },
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

void
brgphy_load_dspcode(sc)
	struct mii_softc *sc;
{
	const struct bcm_dspcode *dsp = NULL;
	int id2, i;

	id2 = PHY_READ(sc, MII_PHYIDR2);

	mii_phy_reset(sc);
	
	switch (MII_MODEL(id2)) {
	case MII_MODEL_BROADCOM_BCM5400:
		dsp = bcm5401_dspcode;
		break;
	case MII_MODEL_BROADCOM_BCM5401:
		if (MII_REV(id2) == 1 || MII_REV(id2) == 3)
			dsp = bcm5401_dspcode;
		break;
	case MII_MODEL_BROADCOM_BCM5411:
		dsp = bcm5411_dspcode;
		break;
	}

	if (dsp == NULL)
		return;

	for (i = 0; dsp[i].reg != 0; i++)
		PHY_WRITE(sc, dsp[i].reg, dsp[i].val);
}
