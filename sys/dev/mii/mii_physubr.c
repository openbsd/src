/*	$OpenBSD: mii_physubr.c,v 1.14 2002/11/26 06:01:28 nate Exp $	*/
/*	$NetBSD: mii_physubr.c,v 1.20 2001/04/13 23:30:09 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/route.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Media to register setting conversion table.  Order matters.
 * XXX 802.3 doesn't specify ANAR or ANLPAR bits for 1000base.
 */
const struct mii_media mii_media_table[] = {
	{ BMCR_ISO,		ANAR_CSMA },		/* None */
	{ BMCR_S10,		ANAR_CSMA|ANAR_10 },	/* 10baseT */
	{ BMCR_S10|BMCR_FDX,	ANAR_CSMA|ANAR_10_FD },	/* 10baseT-FDX */
	{ BMCR_S100,		ANAR_CSMA|ANAR_T4 },	/* 100baseT4 */
	{ BMCR_S100,		ANAR_CSMA|ANAR_TX },	/* 100baseTX */
	{ BMCR_S100|BMCR_FDX,	ANAR_CSMA|ANAR_TX_FD },	/* 100baseTX-FDX */
	{ BMCR_S1000,		ANAR_CSMA },		/* 1000base */
	{ BMCR_S1000|BMCR_FDX,	ANAR_CSMA },		/* 1000base-FDX */
};

void	mii_phy_auto_timeout(void *);

void
mii_phy_setmedia(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, anar;

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		if ((PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN) == 0)
			(void) mii_phy_auto(sc, 1);
		return;
	}

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
mii_phy_auto(sc, waitfor)
	struct mii_softc *sc;
	int waitfor;
{
	int bmsr, i;

	if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		PHY_WRITE(sc, MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA);
		PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(sc, MII_BMSR)) & BMSR_ACOMP)
				return (0);
			delay(1000);
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
	if (sc->mii_flags & MIIF_AUTOTSLEEP) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		tsleep(&sc->mii_flags, PZERO, "miiaut", hz >> 1);
		mii_phy_auto_timeout(sc);
	} else if ((sc->mii_flags & MIIF_DOINGAUTO) == 0) {
		sc->mii_flags |= MIIF_DOINGAUTO;
		timeout_set(&sc->mii_phy_timo, mii_phy_auto_timeout, sc);
		timeout_add(&sc->mii_phy_timo, hz / 2);
	}
	return (EJUSTRETURN);
}

void
mii_phy_auto_timeout(arg)
	void *arg;
{
	struct mii_softc *sc = arg;
	int s, bmsr;

	if ((sc->mii_dev.dv_flags & DVF_ACTIVE) == 0)
		return;

	s = splnet();
	sc->mii_flags &= ~MIIF_DOINGAUTO;
	bmsr = PHY_READ(sc, MII_BMSR);

	/* Update the media status. */
	(void) (*sc->mii_service)(sc, sc->mii_pdata, MII_POLLSTAT);
	splx(s);
}

int
mii_phy_tick(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	/* Just bail now if the interface is down. */
	if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
		return (EJUSTRETURN);

	/*
	 * If we're not doing autonegotiation, we don't need to do
	 * any extra work here.  However, we need to check the link
	 * status so we can generate an announcement if the status
	 * changes.
	 */
	if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
		return (0);

	/* Read the status register twice; BMSR_LINK is latch-low. */
	reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (reg & BMSR_LINK) {
		/*
		 * See above.
		 */
		return (0);
	}

	/*
	 * Only retry autonegotiation every N seconds.
	 */
	if (!sc->mii_anegticks)
		sc->mii_anegticks = 5;

	if (++sc->mii_ticks != sc->mii_anegticks)
		return (EJUSTRETURN);

	sc->mii_ticks = 0;
	mii_phy_reset(sc);

	if (mii_phy_auto(sc, 0) == EJUSTRETURN)
		return (EJUSTRETURN);

	/*
	 * Might need to generate a status message if autonegotiation
	 * failed.
	 */
	return (0);
}

void
mii_phy_reset(sc)
	struct mii_softc *sc;
{
	int reg, i;

	if (sc->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		delay(1000);
	}

	if (sc->mii_inst != 0 && ((sc->mii_flags & MIIF_NOISOLATE) == 0))
		PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
}

void
mii_phy_down(sc)
	struct mii_softc *sc;
{
	if (sc->mii_flags & MIIF_DOINGAUTO) {
		sc->mii_flags &= ~MIIF_DOINGAUTO;
		timeout_del(&sc->mii_phy_timo);
	}
}


void
mii_phy_status(sc)
	struct mii_softc *sc;
{

	(*sc->mii_status)(sc);
}

void
mii_phy_update(sc, cmd)
	struct mii_softc *sc;
	int cmd;
{
	struct mii_data *mii = sc->mii_pdata;

	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		(*mii->mii_statchg)(sc->mii_dev.dv_parent);
		mii_phy_statusmsg(sc);
		sc->mii_media_active = mii->mii_media_active;
		sc->mii_media_status = mii->mii_media_status;
	}
}

void
mii_phy_statusmsg(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifnet *ifp = mii->mii_ifp;
	int s, baudrate, link_state, announce = 0;

	if (mii->mii_media_status & IFM_AVALID) {
		if (mii->mii_media_status & IFM_ACTIVE)
			link_state = LINK_STATE_UP;
		else
			link_state = LINK_STATE_DOWN;
	} else
		link_state = LINK_STATE_UNKNOWN;

	baudrate = ifmedia_baudrate(mii->mii_media_active);

	if (link_state != ifp->if_link_state) {
		ifp->if_link_state = link_state;
		/*
		 * XXX Right here we'd like to notify protocols
		 * XXX that the link status has changed, so that
		 * XXX e.g. Duplicate Address Detection can restart.
		 */
		announce = 1;
	}

	if (baudrate != ifp->if_baudrate) {
		ifp->if_baudrate = baudrate;
		announce = 1;
	}

	if (announce) {
		s = splnet();
		rt_ifmsg(ifp);
		splx(s);
	}
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_phy_add_media(sc)
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
	}
	if (sc->mii_capabilities & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_10_T_FDX);
	}
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
	}
	if (sc->mii_capabilities & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
	}
	if (sc->mii_capabilities & BMSR_100T4) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    MII_MEDIA_100_T4);
	}
	if (sc->mii_extcapabilities & EXTSR_MEDIAMASK) {
		/*
		 * XXX Right now only handle 1000SX and 1000TX.  Need
		 * XXX to hnalde 1000LX and 1000CX some how.
		 */
		if (sc->mii_extcapabilities & EXTSR_1000XHDX) {
			sc->mii_anegticks = 10;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, 0,
			    sc->mii_inst), MII_MEDIA_1000_X);
		}
		if (sc->mii_extcapabilities & EXTSR_1000XFDX) {
			sc->mii_anegticks = 10;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_X_FDX);
		}
		if (sc->mii_extcapabilities & EXTSR_1000THDX) {
			sc->mii_anegticks = 10;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0,
			    sc->mii_inst), MII_MEDIA_1000_T);
		}
		if (sc->mii_extcapabilities & EXTSR_1000TFDX) {
			sc->mii_anegticks = 10;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_T_FDX);
		}
	}

	if (sc->mii_capabilities & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    MII_NMEDIA);	/* intentionally invalid index */
	}
#undef ADD
}

void
mii_phy_delete_media(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;

	ifmedia_delete_instance(&mii->mii_media, sc->mii_inst);
}

int
mii_phy_activate(self, act)
	struct device *self;
	enum devact act;
{
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		/* Nothing special to do. */
		break;
	}

	return (rv);
}

int
mii_phy_detach(self, flags)
	struct device *self;
	int flags;
{
	struct mii_softc *sc = (void *) self;

	if (sc->mii_flags & MIIF_DOINGAUTO)
		timeout_del(&sc->mii_phy_timo);

	mii_phy_delete_media(sc);

	return (0);
}

/*
 * Given an ifmedia word, return the corresponding ANAR value.
 */
int
mii_anar(media)
	int media;
{
	int rv;

	switch (media & (IFM_TMASK|IFM_NMASK|IFM_FDX)) {
	case IFM_ETHER|IFM_10_T:
		rv = ANAR_10|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_10_T|IFM_FDX:
		rv = ANAR_10_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX:
		rv = ANAR_TX|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX|IFM_FDX:
		rv = ANAR_TX_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_T4:
		rv = ANAR_T4|ANAR_CSMA;
		break;
	default:
		rv = 0;
		break;
	}

	return (rv);
}
