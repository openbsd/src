/*	$OpenBSD: exphy.c,v 1.1 1998/09/10 17:17:33 jason Exp $	*/
/*	$NetBSD: exphy.c,v 1.5 1998/08/28 12:50:36 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Frank van der Linden.
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
 * driver for 3Com internal PHYs
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

struct exphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_capabilities;
	int sc_active;
};

#ifdef __NetBSD__
int	exphymatch __P((struct device *, struct cfdata *, void *));
#else
int	exphymatch __P((struct device *, void *, void *));
#endif
void	exphyattach __P((struct device *, struct device *, void *));

struct cfattach exphy_ca = {
	sizeof(struct exphy_softc), exphymatch, exphyattach
};

#ifdef __OpenBSD__
struct cfdriver exphy_cd = {
	NULL, "exphy", DV_DULL
};
#endif

#define	EXPHY_READ(sc, reg) \
    (*(sc)->sc_mii.mii_pdata->mii_readreg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg))

#define	EXPHY_WRITE(sc, reg, val) \
    (*(sc)->sc_mii.mii_pdata->mii_writereg)((sc)->sc_mii.mii_dev.dv_parent, \
	(sc)->sc_mii.mii_phy, (reg), (val))

int	exphy_service __P((struct mii_softc *, struct mii_data *, int));
void	exphy_reset __P((struct exphy_softc *));
void	exphy_auto __P((struct exphy_softc *));
void	exphy_status __P((struct exphy_softc *));

int
exphymatch(parent, match, aux)
	struct device *parent;
#ifdef __NetBSD__
	struct cfdata *match;
#else
	void *match;
#endif
	void *aux;
{
	struct mii_attach_args *ma = aux;

	/*
	 * Argh, 3Com PHY reports oui == 0 model == 0!
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) == 0 &&
	    MII_MODEL(ma->mii_id2) == 0)
		return (1);

	return (0);
}

void
exphyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct exphy_softc *sc = (struct exphy_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;

	printf(": 3Com internal media interface, rev. %d\n",
	    MII_REV(ma->mii_id2));

	sc->sc_mii.mii_inst = mii->mii_instance;
	sc->sc_mii.mii_phy = ma->mii_phyno;
	sc->sc_mii.mii_service = exphy_service;
	sc->sc_mii.mii_pdata = mii;

	/*
	 * The 3Com PHY can never be isolated, so never allow non-zero
	 * instances!
	 */
	if (mii->mii_instance != 0) {
		printf("%s: ignoring this PHY, non-zero instance\n",
		    sc->sc_mii.mii_dev.dv_xname);
		return;
	}

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->sc_mii.mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->sc_mii.mii_inst),
	    BMCR_LOOP|BMCR_S100);

	exphy_reset(sc);

	sc->sc_capabilities = EXPHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	printf("%s: ", sc->sc_mii.mii_dev.dv_xname);
	if ((sc->sc_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->sc_capabilities, sc->sc_mii.mii_inst);
	printf("\n");
#undef ADD
}

int
exphy_service(self, mii, cmd)
	struct mii_softc *self;
	struct mii_data *mii;
	int cmd;
{
	struct exphy_softc *sc = (struct exphy_softc *)self;

	/*
	 * We can't isolate the 3Com PHY, so it has to be the only one!
	 */
	if (IFM_INST(mii->mii_media.ifm_media) != sc->sc_mii.mii_inst)
		panic("exphy_service: can't isolate 3Com PHY");


	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(mii->mii_media.ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (EXPHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			exphy_auto(sc);
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
			EXPHY_WRITE(sc, MII_ANAR,
			    mii_anar(mii->mii_media.ifm_media));
			EXPHY_WRITE(sc, MII_BMCR,
			    mii->mii_media.ifm_cur->ifm_data);
		}
		break;

	case MII_TICK:
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
		 * The 3Com PHY's autonegotiation doesn't need to be
		 * kicked; it continues in the background.
		 */
		break;
	}

	/* Update the media status. */
	exphy_status(sc);

	/* Callback if something changed. */
	if (sc->sc_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->sc_mii.mii_dev.dv_parent);
		sc->sc_active = mii->mii_media_active;
	}
	return (0);
}

void
exphy_status(sc)
	struct exphy_softc *sc;
{
	struct mii_data *mii = sc->sc_mii.mii_pdata;
	int bmsr, bmcr, anlpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = EXPHY_READ(sc, MII_BMSR) | EXPHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = EXPHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * The 3Com PHY uses the highest-order common bit of
		 * the ANAR and ANLPAR (i.e. best media advertised
		 * both by us and our link partner).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		anlpar = EXPHY_READ(sc, MII_ANAR) & EXPHY_READ(sc, MII_ANLPAR);
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
exphy_auto(sc)
	struct exphy_softc *sc;
{
	int bmsr, i;

	EXPHY_WRITE(sc, MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(sc->sc_capabilities) | ANAR_CSMA);
	EXPHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);

	/* Wait 500ms for it to complete. */
	for (i = 0; i < 500; i++) {
		if ((bmsr = EXPHY_READ(sc, MII_BMSR)) & BMSR_ACOMP)
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
exphy_reset(sc)
	struct exphy_softc *sc;
{
	int reg, i;

	EXPHY_WRITE(sc, MII_BMCR, BMCR_RESET);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = EXPHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	/*
	 * XXX 3Com PHY doesn't set the BMCR properly after
	 * XXX reset, which breaks autonegotiation.
	 */
	EXPHY_WRITE(sc, MII_BMCR, BMCR_S100|BMCR_AUTOEN|BMCR_FDX);
}
