/*	$OpenBSD: mii.c,v 1.3 1999/01/04 03:57:55 jason Exp $	*/
/*	$NetBSD: mii.c,v 1.9 1998/11/05 04:08:02 thorpej Exp $	*/

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
 * MII bus layer, glues MII-capable network interface drivers to sharable
 * PHY drivers.  This exports an interface compatible with BSD/OS 3.0's,
 * plus some NetBSD extensions.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

int	mii_print __P((void *, const char *));
#ifdef __NetBSD__
int	mii_submatch __P((struct device *, struct cfdata *, void *));
#else
int	mii_submatch __P((struct device *, void *, void *));
#endif

#ifdef __OpenBSD__
#define MIICF_PHY		0	/* cf_loc index */
#define MIICF_PHY_DEFAULT	(-1)	/* default phy device */
#endif

/*
 * Helper function used by network interface drivers, attaches PHYs
 * to the network interface driver parent.
 */
void
mii_phy_probe(parent, mii, capmask)
	struct device *parent;
	struct mii_data *mii;
	int capmask;
{
	struct mii_attach_args ma;
	struct mii_softc *child;

	LIST_INIT(&mii->mii_phys);

	for (ma.mii_phyno = 0; ma.mii_phyno < MII_NPHY; ma.mii_phyno++) {
		/*
		 * Check to see if there is a PHY at this address.  If
		 * the register contains garbage, assume no.
		 */
		ma.mii_id1 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR1);
		ma.mii_id2 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR2);
		if ((ma.mii_id1 == 0 || ma.mii_id1 == 0xffff) &&
		    (ma.mii_id2 == 0 || ma.mii_id2 == 0xffff)) {
			/*
			 * ARGH!!  3Com internal PHYs report 0/0 in their
			 * ID registers!  If we spot this, check to see
			 * if the BMSR has reasonable data in it.
			 */
			if (MII_OUI(ma.mii_id1, ma.mii_id2) == 0 &&
			    MII_MODEL(ma.mii_id2) == 0) {
				int bmsr = (*mii->mii_readreg)(parent,
				    ma.mii_phyno, MII_BMSR);
				if (bmsr == 0 || bmsr == 0xffff ||
				    (bmsr & BMSR_MEDIAMASK) == 0)
					continue;
			} else
				continue;
		}

		ma.mii_data = mii;
		ma.mii_capmask = capmask;

		if ((child = (struct mii_softc *)config_found_sm(parent, &ma,
		    mii_print, mii_submatch)) != NULL) {
			/*
			 * Link it up in the parent's MII data.
			 */
			LIST_INSERT_HEAD(&mii->mii_phys, child, mii_list);
			mii->mii_instance++;
		}
	}
}

int
mii_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct mii_attach_args *ma = aux;

	if (pnp != NULL)
		printf("OUI 0x%06x model 0x%04x rev %d at %s",
		    MII_OUI(ma->mii_id1, ma->mii_id2), MII_MODEL(ma->mii_id2),
		    MII_REV(ma->mii_id2), pnp);

	printf(" phy %d", ma->mii_phyno);
	return (UNCONF);
}

#ifdef __NetBSD__
int
mii_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
#else
int
mii_submatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
#endif
	struct mii_attach_args *ma = aux;

	if (ma->mii_phyno != cf->cf_loc[MIICF_PHY] &&
	    cf->cf_loc[MIICF_PHY] != MIICF_PHY_DEFAULT)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
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

/*
 * Given a BMCR value, return the corresponding ifmedia word.
 */
int
mii_media_from_bmcr(bmcr)
	int bmcr;
{
	int rv = IFM_ETHER;

	if (bmcr & BMCR_S100)
		rv |= IFM_100_TX;
	else
		rv |= IFM_10_T;
	if (bmcr & IFM_FDX)
		rv |= IFM_FDX;

	return (rv);
}

/*
 * Media changed; notify all PHYs.
 */
int
mii_mediachg(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;
	int rv;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list)) {
		rv = (*child->mii_service)(child, mii, MII_MEDIACHG);
		if (rv)
			return (rv);
	}
	return (0);
}

/*
 * Call the PHY tick routines, used during autonegotiation.
 */
void
mii_tick(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list))
		(void) (*child->mii_service)(child, mii, MII_TICK);
}

/*
 * Get media status from PHYs.
 */
void
mii_pollstat(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list))
		(void) (*child->mii_service)(child, mii, MII_POLLSTAT);
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_add_media(mii, bmsr, instance)
	struct mii_data *mii;
	int bmsr, instance;
{
	const char *sep = "";

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	if (bmsr & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, instance), 0);
		PRINT("10baseT");
	}
	if (bmsr & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, instance),
		    BMCR_FDX);
		PRINT("10baseT-FDX");
	}
	if (bmsr & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, instance),
		    BMCR_S100);
		PRINT("100baseTX");
	}
	if (bmsr & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, instance),
		    BMCR_S100|BMCR_FDX);
		PRINT("100baseTX-FDX");
	}
	if (bmsr & BMSR_100T4) {
		/*
		 * XXX How do you enable 100baseT4?  I assume we set
		 * XXX BMCR_S100 and then assume the PHYs will take
		 * XXX watever action is necessary to switch themselves
		 * XXX into T4 mode.
		 */
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, instance),
		    BMCR_S100);
		PRINT("100baseT4");
	}
	if (bmsr & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, instance),
		    BMCR_AUTOEN);
		PRINT("auto");
	}
#undef ADD
#undef PRINT
}
