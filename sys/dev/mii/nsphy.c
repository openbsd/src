/*	$OpenBSD: nsphy.c,v 1.1 1998/09/10 17:17:34 jason Exp $	*/
/*	$NetBSD: nsphy.c,v 1.9 1998/08/14 00:23:26 thorpej Exp $	*/

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

struct nsphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_capabilities;
	int sc_ticks;
	int sc_active;
};

#ifdef __NetBSD__
int	nsphymatch __P((struct device *, struct cfdata *, void *));
#else
int	nsphymatch __P((struct device *, void *, void *));
#endif
void	nsphyattach __P((struct device *, struct device *, void *));

struct cfattach nsphy_ca = {
	sizeof(struct nsphy_softc), nsphymatch, nsphyattach
};

#ifdef __OpenBSD__
struct cfdriver nsphy_cd = {
	NULL, "nsphy", DV_DULL
};
#endif

#define	NSPHY_READ(sc, reg) \
    (*(sc)->sc_mii.mii_pdata->mii_readreg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg))

#define	NSPHY_WRITE(sc, reg, val) \
    (*(sc)->sc_mii.mii_pdata->mii_writereg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg), (val))

int	nsphy_service __P((struct mii_softc *, struct mii_data *, int));
void	nsphy_reset __P((struct nsphy_softc *));
void	nsphy_auto __P((struct nsphy_softc *));
void	nsphy_status __P((struct nsphy_softc *));

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
		return (1);

	return (0);
}

void
nsphyattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct nsphy_softc *sc = (struct nsphy_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": %s, rev. %d\n", MII_STR_NATSEMI_DP83840,
	    MII_REV(ma->mii_id2));

	sc->sc_mii.mii_inst = mii->mii_instance;
	sc->sc_mii.mii_phy = ma->mii_phyno;
	sc->sc_mii.mii_service = nsphy_service;
	sc->sc_mii.mii_pdata = mii;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

#if 0
	/* Can't do this on the i82557! */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->sc_mii.mii_inst),
	    BMCR_ISO);
#endif
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->sc_mii.mii_inst),
	    BMCR_LOOP|BMCR_S100);

	nsphy_reset(sc);

	sc->sc_capabilities = NSPHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	printf("%s: ", sc->sc_mii.mii_dev.dv_xname);
	if ((sc->sc_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->sc_capabilities, sc->sc_mii.mii_inst);
	printf("\n");
#undef ADD
}

int
nsphy_service(self, mii, cmd)
	struct mii_softc *self;
	struct mii_data *mii;
	int cmd;
{
	struct nsphy_softc *sc = (struct nsphy_softc *)self;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(mii->mii_media.ifm_media) !=
		    sc->sc_mii.mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(mii->mii_media.ifm_media) !=
		    sc->sc_mii.mii_inst) {
			reg = NSPHY_READ(sc, MII_BMCR);
			NSPHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		reg = NSPHY_READ(sc, MII_NSPHY_PCR);

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

		NSPHY_WRITE(sc, MII_NSPHY_PCR, reg);

		switch (IFM_SUBTYPE(mii->mii_media.ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (NSPHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			nsphy_auto(sc);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			NSPHY_WRITE(sc, MII_ANAR,
			    mii_anar(mii->mii_media.ifm_media));
			NSPHY_WRITE(sc, MII_BMCR,
			    mii->mii_media.ifm_cur->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(mii->mii_media.ifm_media) !=
		    sc->sc_mii.mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(mii->mii_media.ifm_media) != IFM_AUTO)
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
		reg = NSPHY_READ(sc, MII_BMSR) | NSPHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->sc_ticks != 5)
			return (0);

		sc->sc_ticks = 0;
		nsphy_reset(sc);
		nsphy_auto(sc);
		break;
	}

	/* Update the media status. */
	nsphy_status(sc);

	/* Callback if something changed. */
	if (sc->sc_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->sc_mii.mii_dev.dv_parent);
		sc->sc_active = mii->mii_media_active;
	}
	return (0);
}

void
nsphy_status(sc)
	struct nsphy_softc *sc;
{
	struct mii_data *mii = sc->sc_mii.mii_pdata;
	int bmsr, bmcr, par, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = NSPHY_READ(sc, MII_BMSR) | NSPHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = NSPHY_READ(sc, MII_BMCR);
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
		if (NSPHY_READ(sc, MII_ANER) & ANER_LPAN) {
			anlpar = NSPHY_READ(sc, MII_ANAR) &
			    NSPHY_READ(sc, MII_ANLPAR);
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
		par = NSPHY_READ(sc, MII_NSPHY_PAR);
		if (par & PAR_10)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_100_TX;
#if 0
		if (par & PAR_FDX)
			mii->mii_media_active |= IFM_FDX;
#endif
	} else {
		if (bmcr & BMCR_S100)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (bmcr & BMCR_FDX)
			mii->mii_media_active |= IFM_FDX;
	}
}

void
nsphy_auto(sc)
	struct nsphy_softc *sc;
{
	int bmsr, i;

	NSPHY_WRITE(sc, MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(sc->sc_capabilities) | ANAR_CSMA);
	NSPHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);

	/* Wait 500ms for it to complete. */
	for (i = 0; i < 500; i++) {
		if ((bmsr = NSPHY_READ(sc, MII_BMSR)) & BMSR_ACOMP)
			return;
		delay(1000);
	}
#if 0
	if ((bmsr & BMSR_ACOMP) == 0)
		printf("%s: autonegotiation failed to complete\n",
		    sc->sc_mii.mii_dev.dv_xname);
#endif
}

void
nsphy_reset(sc)
	struct nsphy_softc *sc;
{
	int reg, i;

	/*
	 * The i82557 wedges if we isolate all of its PHYs!
	 */
	if (sc->sc_mii.mii_inst == 0)
		NSPHY_WRITE(sc, MII_BMCR, BMCR_RESET);
	else
		NSPHY_WRITE(sc, MII_BMCR, BMCR_RESET|BMCR_ISO);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = NSPHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	/* Make sure the PHY is isolated. */
	if (sc->sc_mii.mii_inst != 0)
		NSPHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}
