/*	$OpenBSD: mii_physubr.c,v 1.3 1999/12/07 22:01:31 jason Exp $	*/
/*	$NetBSD: mii_physubr.c,v 1.2.6.1 1999/04/23 15:40:26 perry Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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
 * Subroutines common to all PHYs.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Media to register setting conversion table.  Order matters.
 */
const struct mii_media mii_media_table[] = {
	{ BMCR_ISO,		ANAR_CSMA },		/* None */
	{ 0,			ANAR_CSMA|ANAR_10 },	/* 10baseT */
	{ BMCR_FDX,		ANAR_CSMA|ANAR_10_FD },	/* 10baseT-FDX */
	{ BMCR_S100,		ANAR_CSMA|ANAR_T4 },	/* 100baseT4 */
	{ BMCR_S100,		ANAR_CSMA|ANAR_TX },	/* 100baseTX */
	{ BMCR_S100|BMCR_FDX,	ANAR_CSMA|ANAR_TX_FD }, /* 100baseTX-FDX */
};

void	mii_phy_auto_timeout __P((void *));

void
mii_phy_setmedia(sc)
	struct mii_softc *sc;
{
	struct mii_data*mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, anar;

	/*
	 * Table index is stored in the media entry.
	 */

#ifdef DIAGNOSTIC
	if (ife->ifm_data < 0 || ife->ifm_data >= MII_NMEDIA)
		panic("mii_phy_setmedia");
#endif

	anar = mii_media_table[ife->ifm_data].mm_anar;
	bmcr = mii_media_table[ife->ifm_data].mm_bmcr;

	if (ife->ifm_media & IFM_LOOP)
		bmcr |= BMCR_LOOP;

	PHY_WRITE(sc, MII_ANAR, anar);
	PHY_WRITE(sc, MII_BMCR, bmcr);
}

int
mii_phy_auto(mii, waitfor)
	struct mii_softc *mii;
{
	int bmsr, i;

	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_WRITE(mii, MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
		PHY_WRITE(mii, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(mii, MII_BMSR)) & BMSR_ACOMP)
				return (0);
			delay(1000);
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
		timeout(mii_phy_auto_timeout, mii, hz >> 1);
	}
	return (EJUSTRETURN);
}

void
mii_phy_auto_timeout(arg)
	void *arg;
{
	struct mii_softc *mii = arg;
	int s, bmsr;

	s = splnet();
	mii->mii_flags &= ~MIIF_DOINGAUTO;
	bmsr = PHY_READ(mii, MII_BMSR);

	/* Update the media status. */
	(void) (*mii->mii_service)(mii, mii->mii_pdata, MII_POLLSTAT);
	splx(s);
}

void
mii_phy_reset(mii)
	struct mii_softc *mii;
{
	int reg, i;

	if (mii->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(mii, MII_BMCR, reg);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(mii, MII_BMCR); 
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	if (mii->mii_inst != 0 && ((mii->mii_flags & MIIF_NOISOLATE) == 0))
		PHY_WRITE(mii, MII_BMCR, reg | BMCR_ISO);
}
void
mii_phy_down(sc)
	struct mii_softc *sc;
{
	if (sc->mii_flags & MIIF_DOINGAUTO) {
		sc->mii_flags &= ~MIIF_DOINGAUTO;
		untimeout(mii_phy_auto_timeout, sc);
	}
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_add_media(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
		    MII_MEDIA_NONE);

	if (sc->mii_capabilities & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    MII_MEDIA_10_T);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_10_T);
#endif
	}

	if (sc->mii_capabilities & BMSR_10TFDX)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_10_T_FDX);
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_100_T4);
#endif
	}
	if (sc->mii_capabilities & BMSR_100TXFDX)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
	if (sc->mii_capabilities & BMSR_100T4) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    MII_MEDIA_100_T4);
#if 0
		if ((sc->mii_flags & MIIF_NOLOOP) == 0)
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, IFM_LOOP,
			    sc->mii_inst), MII_MEDIA_100_T4);
#endif
	}
	if (sc->mii_capabilities & BMSR_ANEG)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    MII_NMEDIA);	/* intentionally invalid index */
#undef ADD
}
