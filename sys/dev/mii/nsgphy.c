/*	$OpenBSD: nsgphy.c,v 1.14 2005/02/19 06:00:04 brad Exp $	*/
/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 2001
 *	Bill Paul <wpaul@bsdi.com>.  All rights reserved.
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
 */

/*
 * Driver for the National Semiconductor DP83891 and DP83861
 * 10/100/1000 PHYs.
 * Datasheet available at: http://www.national.com/ds/DP/DP83861.pdf
 *
 * The DP83891 is the older NatSemi gigE PHY which isn't being sold
 * anymore. The DP83861 is its replacement, which is an 'enhanced'
 * firmware driven component. The major difference between the
 * two is that the 83891 can't generate interrupts, while the
 * 83861 can. (I think it wasn't originally designed to do this, but
 * it can now thanks to firmware updates.) The 83861 also allows
 * access to its internal RAM via indirect register access.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/nsgphyreg.h>

int	nsgphymatch(struct device*, void *, void *);
void	nsgphyattach(struct device *, struct device *, void *);

struct cfattach nsgphy_ca = {
	sizeof(struct mii_softc), nsgphymatch, nsgphyattach, mii_phy_detach,
	mii_phy_activate
};

struct cfdriver nsgphy_cd = {
	NULL, "nsgphy", DV_DULL
};

int	nsgphy_service(struct mii_softc *, struct mii_data *, int);
void	nsgphy_status(struct mii_softc *);

static int	nsgphy_mii_phy_auto(struct mii_softc *, int);
extern void	mii_phy_auto_timeout(void *);

const struct mii_phy_funcs nsgphy_funcs = {
	nsgphy_service, nsgphy_status, mii_phy_reset,
};

static const struct mii_phydesc nsgphys[] = {
	{ MII_OUI_NATSEMI,		MII_MODEL_NATSEMI_DP83861,
	  MII_STR_NATSEMI_DP83861 },
	{ MII_OUI_NATSEMI,		MII_MODEL_NATSEMI_DP83891,
	  MII_STR_NATSEMI_DP83891 },

	{ 0,			0,
	  NULL },
};

int
nsgphymatch(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, nsgphys) != NULL)
		return (10);

	return (0);
}

void
nsgphyattach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, nsgphys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &nsgphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = 10;

	PHY_RESET(sc);

	sc->mii_capabilities =
		PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
        if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
        if ((sc->mii_capabilities & BMSR_MEDIAMASK) ||
            (sc->mii_extcapabilities & EXTSR_MEDIAMASK))
                mii_phy_add_media(sc);

#if 0
        strap = PHY_READ(sc, MII_GPHYTER_STRAP);
        printf("%s: strapped to %s mode", sc->mii_dev.dv_xname,
	       (strap & STRAP_MS_VAL) ? "master" : "slave");
        if (strap & STRAP_NC_MODE)
                printf(", pre-C5 BCM5400 compat enabled");
        printf("\n");
#endif
}

int
nsgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			(void) nsgphy_mii_phy_auto(sc, 0);
			break;
		case IFM_1000_T:
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, NSGPHY_MII_BMCR,
				    NSGPHY_BMCR_FDX|NSGPHY_BMCR_SPD1);
			} else {
				PHY_WRITE(sc, NSGPHY_MII_BMCR,
				    NSGPHY_BMCR_SPD1);
			}
			PHY_WRITE(sc, NSGPHY_MII_ANAR, NSGPHY_SEL_TYPE);

			/*
			 * When setting the link manually, one side must
			 * be the master and the other the slave. However
			 * ifmedia doesn't give us a good way to specify
			 * this, so we fake it by using one of the LINK
			 * flags. If LINK0 is set, we program the PHY to
			 * be a master, otherwise it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, NSGPHY_MII_1000CTL,
				    NSGPHY_1000CTL_MSE|NSGPHY_1000CTL_MSC);
			} else {
				PHY_WRITE(sc, NSGPHY_MII_1000CTL,
				    NSGPHY_1000CTL_MSE);
			}
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO|BMCR_PDOWN);
			break;
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
			break;
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
		 * Only retry autonegotiation if we've hit the timeout.
		 */
		if (++sc->mii_ticks != sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;

		/*
		 * Check to see if we have link.
		 */
		reg = PHY_READ(sc, NSGPHY_MII_PHYSUP);
		if (reg & NSGPHY_PHYSUP_LNKSTS)
			break;

		PHY_RESET(sc);
		if (nsgphy_mii_phy_auto(sc, 0) == EJUSTRETURN)
			return(0);
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
nsgphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, physup, anlpar, gstat;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, NSGPHY_MII_BMSR) | PHY_READ(sc, NSGPHY_MII_BMSR);
	physup = PHY_READ(sc, NSGPHY_MII_PHYSUP);
	if (physup & NSGPHY_PHYSUP_LNKSTS)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, NSGPHY_MII_BMCR);

	if (bmcr & NSGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & NSGPHY_BMCR_AUTOEN) {
		if ((bmsr & NSGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		anlpar = PHY_READ(sc, NSGPHY_MII_ANLPAR);
		gstat = PHY_READ(sc, NSGPHY_MII_1000STS);
		if (gstat & NSGPHY_1000STS_LPFD)
			mii->mii_media_active |= IFM_1000_T|IFM_FDX;
		else if (gstat & NSGPHY_1000STS_LPHD)
			mii->mii_media_active |= IFM_1000_T|IFM_HDX;
		else if (anlpar & NSGPHY_ANLPAR_100T4)
			mii->mii_media_active |= IFM_100_T4;
		else if (anlpar & NSGPHY_ANLPAR_100FDX)
			mii->mii_media_active |= IFM_100_TX|IFM_FDX;
		else if (anlpar & NSGPHY_ANLPAR_100HDX)
			mii->mii_media_active |= IFM_100_TX;
		else if (anlpar & NSGPHY_ANLPAR_10FDX)
			mii->mii_media_active |= IFM_10_T|IFM_FDX;
		else if (anlpar & NSGPHY_ANLPAR_10HDX)
			mii->mii_media_active |= IFM_10_T|IFM_HDX;
		else
			mii->mii_media_active |= IFM_NONE;
		return;
	}

	switch(bmcr & (NSGPHY_BMCR_SPD1|NSGPHY_BMCR_SPD0)) {
	case NSGPHY_S1000:
		mii->mii_media_active |= IFM_1000_T;
		break;
	case NSGPHY_S100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case NSGPHY_S10:
		mii->mii_media_active |= IFM_10_T;
		break;
	default:
		break;
	}

	if (bmcr & NSGPHY_BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	return;
}


static int
nsgphy_mii_phy_auto(struct mii_softc *sc, int waitfor)
{
	int bmsr, ktcr = 0, i;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_RESET(sc);
		PHY_WRITE(sc, NSGPHY_MII_BMCR, 0);
		DELAY(1000);
		ktcr = PHY_READ(sc, NSGPHY_MII_1000CTL);
		PHY_WRITE(sc, NSGPHY_MII_1000CTL, ktcr |
		    (NSGPHY_1000CTL_AFD|NSGPHY_1000CTL_AHD));
		ktcr = PHY_READ(sc, NSGPHY_MII_1000CTL);
		DELAY(1000);
		PHY_WRITE(sc, NSGPHY_MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA);
		DELAY(1000);
		PHY_WRITE(sc, NSGPHY_MII_BMCR,
		    NSGPHY_BMCR_AUTOEN | NSGPHY_BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(sc, NSGPHY_MII_BMSR)) &
			    NSGPHY_BMSR_ACOMP)
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
	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		timeout_set(&sc->mii_phy_timo, mii_phy_auto_timeout, sc);
		timeout_add(&sc->mii_phy_timo, hz >> 1);
	}
	return (EJUSTRETURN);
}
