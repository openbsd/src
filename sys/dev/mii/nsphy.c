/*	$OpenBSD: nsphy.c,v 1.4 1999/01/04 04:44:05 jason Exp $	*/
/*	$NetBSD: nsphy.c,v 1.16 1998/11/05 04:08:02 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * driver for National Semiconductor's DP83840A ethernet 10/100 PHY
 * Data Sheet available from www.national.com
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/nsphyreg.h>

#ifdef __NetBSD__
int	nsphymatch __P((struct device *, struct cfdata *, void *));
#else
int	nsphymatch __P((struct device *, void *, void *));
#endif
void	nsphyattach __P((struct device *, struct device *, void *));

struct cfattach nsphy_ca = {
	sizeof(struct mii_softc), nsphymatch, nsphyattach
};

#ifdef __OpenBSD__
struct cfdriver nsphy_cd = {
	NULL, "nsphy", DV_DULL
};
#endif

int	nsphy_service __P((struct mii_softc *, struct mii_data *, int));
void	nsphy_status __P((struct mii_softc *));
void	nsphy_reset __P((struct mii_softc *));

int
nsphymatch(parent, match, aux)
	struct device *parent;
#ifdef __NetBSD__
	struct cfdata *match;
#else
	void *match;
#endif
	void *aux;
{
	struct mii_attach_args *ma = aux;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_NATSEMI &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_NATSEMI_DP83840)
		return (10);

	return (0);
}

void
nsphyattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": %s, rev. %d\n", MII_STR_NATSEMI_DP83840,
	    MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = nsphy_service;
	sc->mii_pdata = mii;

	/*
	 * i82557 wedges if all of its PHYs are isolated!
	 */
	if (strcmp(parent->dv_cfdata->cf_driver->cd_name, "fxp") == 0 &&
	    mii->mii_instance == 0)
		sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

#if 0
	/* Can't do this on the i82557! */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
#endif
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);

	nsphy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	printf("%s: ", sc->mii_dev.dv_xname);
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->mii_capabilities,
		    sc->mii_inst);
	printf("\n");
#undef ADD
}

int
nsphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

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

		reg = PHY_READ(sc, MII_NSPHY_PCR);

		/*
		 * Set up the PCR to use LED4 to indicate full-duplex
		 * in both 10baseT and 100baseTX modes.
		 */
		reg |= PCR_LED4MODE;

		/*
		 * Make sure Carrier Intgrity Monitor function is
		 * disabled (normal for Node operation, but sometimes
		 * it's not set?!)
		 */
		reg |= PCR_CIMDIS;

		/*
		 * Make sure "force link good" is not set.  It's only
		 * intended for debugging, but sometimes it's set
		 * after a reset.
		 */
		reg &= ~PCR_FLINK100;

#if 0
		/*
		 * Mystery bits which are supposedly `reserved',
		 * but we seem to need to set them when the PHY
		 * is connected to some interfaces!
		 */
		reg |= 0x0100 | 0x0400;
#endif

		PHY_WRITE(sc, MII_NSPHY_PCR, reg);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			nsphy_reset(sc);
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			(void) mii_phy_auto(sc);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		case IFM_100_TX:
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));

			reg = 0;
			reg |= BMCR_S100;
			if ((sc->mii_flags & MIIF_NOISOLATE) == 0)
				reg |= BMCR_ISO;

			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX)
				reg |= BMCR_FDX;
			PHY_WRITE(sc, MII_BMCR, reg);
			delay(1000000);		/* XXX too long, will adjust */

			reg &= ~BMCR_ISO;
			PHY_WRITE(sc, MII_BMCR, reg);
			break;

		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
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
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, MII_BMSR) |
		    PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;
		nsphy_reset(sc);
		(void) mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	nsphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->mii_dev.dv_parent);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

void
nsphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, par, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) |
	    PHY_READ(sc, MII_BMSR);
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
		 * The PAR status bits are only valid of autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		/*
		 * Argh.  The PAR doesn't seem to indicate duplex mode
		 * properly!  Determine media based on link partner's
		 * advertised capabilities.
		 */
		if (PHY_READ(sc, MII_ANER) & ANER_LPAN) {
			anlpar = PHY_READ(sc, MII_ANAR) &
			    PHY_READ(sc, MII_ANLPAR);
			if (anlpar & ANLPAR_T4)
				mii->mii_media_active |= IFM_100_T4;
			else if (anlpar & ANLPAR_TX_FD)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_TX)
				mii->mii_media_active |= IFM_100_TX;
			else if (anlpar & ANLPAR_10_FD)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE;
			return;
		}

		/*
		 * Link partner is not capable of autonegotiation.
		 * We will never be in full-duplex mode if this is
		 * the case, so reading the PAR is OK.
		 */
		par = PHY_READ(sc, MII_NSPHY_PAR);
		if (par & PAR_10)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_100_TX;
#if 0
		if (par & PAR_FDX)
			mii->mii_media_active |= IFM_FDX;
#endif
	} else
		mii->mii_media_active = mii_media_from_bmcr(bmcr);
}

void
nsphy_reset(sc)
	struct mii_softc *sc;
{
	int anar;

	mii_phy_reset(sc);
	anar = PHY_READ(sc, MII_ANAR);
	anar |= BMSR_MEDIA_TO_ANAR(PHY_READ(sc, MII_BMSR));
	PHY_WRITE(sc, MII_ANAR, anar);
}
