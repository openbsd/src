/*	$OpenBSD: eephy.c,v 1.15 2005/02/05 19:11:34 brad Exp $	*/
/*
 * Principal Author: Parag Patel
 * Copyright (c) 2001
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Additonal Copyright (c) 2001 by Traakan Software under same licence.
 * Secondary Author: Matthew Jacob
 */

/*
 * driver for the Marvell 88E1000 series external 1000/100/10-BT PHY.
 */

/*
 * Support added for the Marvell 88E1011 (Alaska) 1000/100/10baseTX and
 * 1000baseSX PHY.
 * Nathan Binkert <nate@openbsd.org>
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

#include <dev/mii/eephyreg.h>

int	eephy_service(struct mii_softc *, struct mii_data *, int);
void	eephy_status(struct mii_softc *);
int	eephymatch(struct device *, void *, void *);
void	eephyattach(struct device *, struct device *, void *);

struct cfattach eephy_ca = {
	sizeof (struct mii_softc), eephymatch, eephyattach,
	mii_phy_detach, mii_phy_activate
};

struct cfdriver eephy_cd = {
	NULL, "eephy", DV_DULL
};

int	eephy_mii_phy_auto(struct mii_softc *, int);
void	eephy_reset(struct mii_softc *);

extern void	mii_phy_auto_timeout(void *);

const struct mii_phy_funcs eephy_funcs = {
	eephy_service, eephy_status, eephy_reset,
};

static const struct mii_phydesc eephys[] = {
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1000_3,
	  MII_STR_xxMARVELL_E1000_3 },
	{ MII_OUI_xxMARVELL,		MII_MODEL_xxMARVELL_E1000_5,
	  MII_STR_xxMARVELL_E1000_5 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000,
	  MII_STR_MARVELL_E1000 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1011,
	  MII_STR_MARVELL_E1011 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000_3,
	  MII_STR_MARVELL_E1000_3 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000_4,
	  MII_STR_MARVELL_E1000_4 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000_5,
	  MII_STR_MARVELL_E1000_5 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000_6,
	  MII_STR_MARVELL_E1000_6 },
	{ MII_OUI_MARVELL,		MII_MODEL_MARVELL_E1000_7,
	  MII_STR_MARVELL_E1000_7 },

	{ 0,				0,
	  NULL },
};

int
eephymatch(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if(mii_phy_match(ma, eephys) != NULL)
		return (10);

	return(0);
}

void
eephyattach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, eephys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &eephy_funcs;
	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = 10;

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_MARVELL &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_MARVELL_E1011 && 
	    (PHY_READ(sc, E1000_ESSR) & E1000_ESSR_FIBER_LINK))
		sc->mii_flags |= MIIF_HAVEFIBER;

	PHY_RESET(sc);

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)


	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_1000 | E1000_CR_FULL_DUPLEX);
		/*
		  TODO - apparently 1000BT-simplex not supported?
		  ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0, sc->mii_inst),
		      E1000_CR_SPEED_1000);
		*/
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_100 | E1000_CR_FULL_DUPLEX);
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    E1000_CR_SPEED_100);
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_10 | E1000_CR_FULL_DUPLEX);
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    E1000_CR_SPEED_10);
	} else {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX,sc->mii_inst),
		    E1000_CR_SPEED_1000 | E1000_CR_FULL_DUPLEX);
	}

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);

#undef ADD
}

void
eephy_reset(struct mii_softc *sc)
{
	u_int32_t reg;
	int i;

	/* initialize custom E1000 registers to magic values */
	reg = PHY_READ(sc, E1000_SCR);
	reg &= ~E1000_SCR_AUTO_X_MODE;
	PHY_WRITE(sc, E1000_SCR, reg);

	/* normal PHY reset */
	/*mii_phy_reset(sc);*/
	reg = PHY_READ(sc, E1000_CR);
	reg |= E1000_CR_RESET;
	PHY_WRITE(sc, E1000_CR, reg);

	for (i = 0; i < 500; i++) {
		DELAY(1);
		reg = PHY_READ(sc, E1000_CR);
		if (!(reg & E1000_CR_RESET))
			break;
	}

	/* set more custom E1000 registers to magic values */
	reg = PHY_READ(sc, E1000_SCR);
	reg |= E1000_SCR_ASSERT_CRS_ON_TX;
	PHY_WRITE(sc, E1000_SCR, reg);

	reg = PHY_READ(sc, E1000_ESCR);
	reg |= E1000_ESCR_TX_CLK_25;
	PHY_WRITE(sc, E1000_ESCR, reg);

	/* even more magic to reset DSP? */
	PHY_WRITE(sc, 29, 0x1d);
	PHY_WRITE(sc, 30, 0xc1);
	PHY_WRITE(sc, 30, 0x00);
}

int
eephy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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
			reg = PHY_READ(sc, E1000_CR);
			PHY_WRITE(sc, E1000_CR, reg | E1000_CR_ISOLATE);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0) {
			break;
		}

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (sc->mii_flags & MIIF_DOINGAUTO) {
				return (0);
			}
			PHY_RESET(sc);
			(void)eephy_mii_phy_auto(sc, 1);
			break;

		case IFM_1000_SX:
			PHY_RESET(sc);

			PHY_WRITE(sc, E1000_CR,
			    E1000_CR_FULL_DUPLEX | E1000_CR_SPEED_1000);
			PHY_WRITE(sc, E1000_AR, E1000_FA_1000X_FD);
			break;

		case IFM_1000_T:
			if (sc->mii_flags & MIIF_DOINGAUTO)
				return (0);

			PHY_RESET(sc);

			/* TODO - any other way to force 1000BT? */
			(void)eephy_mii_phy_auto(sc, 1);
			break;

		case IFM_100_TX:
			PHY_RESET(sc);

			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, E1000_CR,
				    E1000_CR_FULL_DUPLEX | E1000_CR_SPEED_100);
				PHY_WRITE(sc, E1000_AR, E1000_AR_100TX_FD);
			} else {
				PHY_WRITE(sc, E1000_CR, E1000_CR_SPEED_100);
				PHY_WRITE(sc, E1000_AR, E1000_AR_100TX);
			}
			break;

		case IFM_10_T:
			PHY_RESET(sc);

			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, E1000_CR,
				    E1000_CR_FULL_DUPLEX | E1000_CR_SPEED_10);
				PHY_WRITE(sc, E1000_AR, E1000_AR_10T_FD);
			} else {
				PHY_WRITE(sc, E1000_CR, E1000_CR_SPEED_10);
				PHY_WRITE(sc, E1000_AR, E1000_AR_10T);
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
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			return (0);
		}

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			return (0);
		}

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0) {
			return (0);
		}

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++(sc->mii_ticks) != sc->mii_anegticks) {
			return (0);
		}
		sc->mii_ticks = 0;

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, E1000_SR) | PHY_READ(sc, E1000_SR);

		if (reg & E1000_SR_LINK_STATUS)
			break;

		PHY_RESET(sc);

		if (eephy_mii_phy_auto(sc, 0) == EJUSTRETURN) {
			return(0);
		}

		break;
	}

	/* Update the media status. */
	eephy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);

	return (0);
}

void
eephy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, esr, ssr, isr, ar, lpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, E1000_SR) | PHY_READ(sc, E1000_SR);
	esr = PHY_READ(sc, E1000_ESR);
	bmcr = PHY_READ(sc, E1000_CR);
	ssr = PHY_READ(sc, E1000_SSR);
	isr = PHY_READ(sc, E1000_ISR);
	ar = PHY_READ(sc, E1000_AR);
	lpar = PHY_READ(sc, E1000_LPAR);

	if (bmsr & E1000_SR_LINK_STATUS)
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & E1000_CR_LOOPBACK)
		mii->mii_media_active |= IFM_LOOP;

	if ((sc->mii_flags & MIIF_DOINGAUTO) &&
	    (!(bmsr & E1000_SR_AUTO_NEG_COMPLETE) || !(ssr & E1000_SSR_LINK) ||
	    !(ssr & E1000_SSR_SPD_DPLX_RESOLVED))) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		if (ssr & E1000_SSR_1000MBS)
			mii->mii_media_active |= IFM_1000_T;
		else if (ssr & E1000_SSR_100MBS)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
	} else {
		if (ssr & E1000_SSR_1000MBS)
			mii->mii_media_active |= IFM_1000_SX;
	}

	if (ssr & E1000_SSR_DUPLEX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		/* FLAG0==rx-flow-control FLAG1==tx-flow-control */
		if ((ar & E1000_AR_PAUSE) && (lpar & E1000_LPAR_PAUSE)) {
			mii->mii_media_active |= IFM_FLAG0 | IFM_FLAG1;
		} else if (!(ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
			   (lpar & E1000_LPAR_PAUSE) &&
			   (lpar & E1000_LPAR_ASM_DIR)) {
			mii->mii_media_active |= IFM_FLAG1;
		} else if ((ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
			   !(lpar & E1000_LPAR_PAUSE) &&
			   (lpar & E1000_LPAR_ASM_DIR)) {
			mii->mii_media_active |= IFM_FLAG0;
		}
	}
}

int
eephy_mii_phy_auto(struct mii_softc *sc, int waitfor)
{
	int bmsr, i;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
			PHY_WRITE(sc, E1000_AR,
				  E1000_AR_10T | E1000_AR_10T_FD |
				  E1000_AR_100TX | E1000_AR_100TX_FD |
				  E1000_AR_PAUSE | E1000_AR_ASM_DIR);
			PHY_WRITE(sc, E1000_1GCR, E1000_1GCR_1000T_FD);
		} else {
			PHY_WRITE(sc, E1000_AR, E1000_FA_1000X_FD |
				  E1000_FA_SYM_PAUSE | E1000_FA_ASYM_PAUSE);
		}
		PHY_WRITE(sc, E1000_CR,
		    E1000_CR_AUTO_NEG_ENABLE | E1000_CR_RESTART_AUTO_NEG);
	}

	if (waitfor) {
		/* Wait 5 seconds for it to complete. */
		for (i = 0; i < 5000; i++) {
			bmsr = PHY_READ(sc, E1000_SR) | PHY_READ(sc, E1000_SR);

			if (bmsr & E1000_SR_AUTO_NEG_COMPLETE) {
				return (0);
			}
			DELAY(1000);
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag. [do it anyway]
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
		sc->mii_ticks = 0;
		timeout_set(&sc->mii_phy_timo, mii_phy_auto_timeout, sc);
		timeout_add(&sc->mii_phy_timo, 5 * hz);
	}
	return (EJUSTRETURN);
}
