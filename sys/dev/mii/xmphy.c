/*	$OpenBSD: xmphy.c,v 1.10 2005/01/28 18:27:55 brad Exp $	*/

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
 * $FreeBSD: src/sys/dev/mii/xmphy.c,v 1.1 2000/04/22 01:58:18 wpaul Exp $
 */

/*
 * driver for the XaQti XMAC II's internal PHY. This is sort of
 * like a 10/100 PHY, except the only thing we're really autoselecting
 * here is full/half duplex. Speed is always 1000mbps.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/xmphyreg.h>

int xmphy_probe(struct device *, void *, void *);
void xmphy_attach(struct device *, struct device *, void *);

struct cfattach xmphy_ca = {
	sizeof(struct mii_softc), xmphy_probe, xmphy_attach, mii_phy_detach,
	    mii_phy_activate
};

struct cfdriver xmphy_cd = {
	NULL, "xmphy", DV_DULL
};

int	xmphy_service(struct mii_softc *, struct mii_data *, int);
void	xmphy_status(struct mii_softc *);

int	xmphy_mii_phy_auto(struct mii_softc *, int);
extern void	mii_phy_auto_timeout(void *);

const struct mii_phy_funcs xmphy_funcs = {
	xmphy_service, xmphy_status, mii_phy_reset,
};

int xmphy_probe(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxXAQTI &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_XAQTI_XMACII)
		return(10);

	return(0);
}

void
xmphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": %s, rev. %d\n", MII_STR_XAQTI_XMACII, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &xmphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = 5;

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	PHY_RESET(sc);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, 0, sc->mii_inst),
	    XMPHY_BMCR_FDX);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX, sc->mii_inst), 0);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);

#undef ADD
}

int
xmphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, XMPHY_MII_BMCR) & XMPHY_BMCR_AUTOEN)
				return (0);
			(void) xmphy_mii_phy_auto(sc, 1);
			break;
		case IFM_1000_SX:
			PHY_RESET(sc);
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, XMPHY_MII_ANAR, XMPHY_ANAR_FDX);
				PHY_WRITE(sc, XMPHY_MII_BMCR, XMPHY_BMCR_FDX);
			} else {
				PHY_WRITE(sc, XMPHY_MII_ANAR, XMPHY_ANAR_HDX);
				PHY_WRITE(sc, XMPHY_MII_BMCR, 0);
			}
			break;
		case IFM_100_T4:
		case IFM_100_TX:
		case IFM_10_T:
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
		reg = PHY_READ(sc, XMPHY_MII_BMSR) |
		    PHY_READ(sc, XMPHY_MII_BMSR);
		if (reg & XMPHY_BMSR_LINK)
			break;

		PHY_RESET(sc);
		if (xmphy_mii_phy_auto(sc, 0) == EJUSTRETURN)
			return(0);
		break;
	}

	/* Update the media status. */
	xmphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

void
xmphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, XMPHY_MII_BMSR) |
	    PHY_READ(sc, XMPHY_MII_BMSR);
	if (bmsr & XMPHY_BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	/* Do dummy read of extended status register. */
	bmcr = PHY_READ(sc, XMPHY_MII_EXTSTS);

	bmcr = PHY_READ(sc, XMPHY_MII_BMCR);

	if (bmcr & XMPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;


	if (bmcr & XMPHY_BMCR_AUTOEN) {
		if ((bmsr & XMPHY_BMSR_ACOMP) == 0) {
			if (bmsr & XMPHY_BMSR_LINK) {
				mii->mii_media_active |= IFM_1000_SX|IFM_HDX;
				return;
			}
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		mii->mii_media_active |= IFM_1000_SX;
		anlpar = PHY_READ(sc, XMPHY_MII_ANAR) &
		    PHY_READ(sc, XMPHY_MII_ANLPAR);
		if (anlpar & XMPHY_ANLPAR_FDX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
		return;
	}

	mii->mii_media_active |= IFM_1000_SX;
	if (bmcr & XMPHY_BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	return;
}


int
xmphy_mii_phy_auto(struct mii_softc *sc, int waitfor)
{
	int bmsr, i;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_WRITE(sc, XMPHY_MII_ANAR,
		    XMPHY_ANAR_FDX|XMPHY_ANAR_HDX);
		PHY_WRITE(sc, XMPHY_MII_BMCR,
		    XMPHY_BMCR_AUTOEN | XMPHY_BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(sc, XMPHY_MII_BMSR)) &
			    XMPHY_BMSR_ACOMP)
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
