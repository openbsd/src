/*	$OpenBSD: mii.c,v 1.13 2004/09/26 00:59:58 brad Exp $	*/
/*	$NetBSD: mii.c,v 1.19 2000/02/02 17:09:44 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
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

int	mii_print(void *, const char *);
int	mii_submatch(struct device *, void *, void *);

#define MIICF_PHY		0	/* cf_loc index */
#define MIICF_PHY_DEFAULT	(-1)	/* default phy device */

/*
 * Helper function used by network interface drivers, attaches PHYs
 * to the network interface driver parent.
 */
void
mii_attach(parent, mii, capmask, phyloc, offloc, flags)
	struct device *parent;
	struct mii_data *mii;
	int capmask, phyloc, offloc, flags;
{
	struct mii_attach_args ma;
	struct mii_softc *child;
	int bmsr, offset = 0;
	int phymin, phymax;

	if (phyloc != MII_PHY_ANY && offloc != MII_PHY_ANY)
		panic("mii_attach: phyloc and offloc specified");

	if (phyloc == MII_PHY_ANY) {
		phymin = 0;
		phymax = MII_NPHY - 1;
	} else
		phymin = phymax = phyloc;

	if ((mii->mii_flags & MIIF_INITDONE) == 0) {
		LIST_INIT(&mii->mii_phys);
		mii->mii_flags |= MIIF_INITDONE;
	}

	for (ma.mii_phyno = phymin; ma.mii_phyno <= phymax; ma.mii_phyno++) {
		/*
		 * Make sure we haven't already configured a PHY at this
		 * address.  This allows mii_attach() to be called
		 * multiple times.
		 */
		for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
		     child = LIST_NEXT(child, mii_list)) {
			if (child->mii_phy == ma.mii_phyno) {
				/*
				 * Yes, there is already something
				 * configured at this address.
				 */
				offset++;
				continue;
			}
		}

		/*
		 * Check to see if there is a PHY at this address.  Note,
		 * many braindead PHYs report 0/0 in their ID registers,
		 * so we test for media in the BMSR.
		 */
		bmsr = (*mii->mii_readreg)(parent, ma.mii_phyno, MII_BMSR);
		if (bmsr == 0 || bmsr == 0xffff ||
		    (bmsr & (BMSR_MEDIAMASK|BMSR_EXTSTAT)) == 0) {
			/* Assume no PHY at this address. */
			continue;
		}

		/*
		 * There is a PHY at this address.  If we were given an
		 * `offset' locator, skip this PHY if it doesn't match.
		 */
		if (offloc != MII_OFFSET_ANY && offloc != offset) {
			offset++;
			continue;
		}

		/*
		 * Extract the IDs.  Braindead PHYs will be handled by
		 * the `ukphy' driver, as we have no ID information to
		 * match on.
		 */
		ma.mii_id1 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR1);
		ma.mii_id2 = (*mii->mii_readreg)(parent, ma.mii_phyno,
		    MII_PHYIDR2);

		ma.mii_data = mii;
		ma.mii_capmask = capmask;
		ma.mii_flags = flags;

		if ((child = (struct mii_softc *)config_found_sm(parent, &ma,
		    mii_print, mii_submatch)) != NULL) {
			/*
			 * Link it up in the parent's MII data.
			 */
#if defined(__NetBSD__)
			callout_init(&child->mii_nway_ch);
#endif
			LIST_INSERT_HEAD(&mii->mii_phys, child, mii_list);
			child->mii_offset = offset;
			mii->mii_instance++;
		}
		offset++;
	}
}

void
mii_activate(mii, act, phyloc, offloc)
	struct mii_data *mii;
	enum devact act;
	int phyloc, offloc;
{
	struct mii_softc *child;

	if (phyloc != MII_PHY_ANY && offloc != MII_PHY_ANY)
		panic("mii_activate: phyloc and offloc specified");

	if ((mii->mii_flags & MIIF_INITDONE) == 0)
		return;

	for (child = LIST_FIRST(&mii->mii_phys);
	     child != NULL; child = LIST_NEXT(child, mii_list)) {
		if (phyloc != MII_PHY_ANY || offloc != MII_OFFSET_ANY) {
			if (phyloc != MII_PHY_ANY &&
			    phyloc != child->mii_phy)
				continue;
			if (offloc != MII_OFFSET_ANY &&
			    offloc != child->mii_offset)
				continue;
		}
		switch (act) {
		case DVACT_ACTIVATE:
			panic("mii_activate: DVACT_ACTIVATE");
			break;

		case DVACT_DEACTIVATE:
			if (config_deactivate(&child->mii_dev) != 0)
				panic("%s: config_activate(%d) failed",
				    child->mii_dev.dv_xname, act);
		}
	}
}

void
mii_detach(mii, phyloc, offloc)
	struct mii_data *mii;
	int phyloc, offloc;
{
	struct mii_softc *child, *nchild;

	if (phyloc != MII_PHY_ANY && offloc != MII_PHY_ANY)
		panic("mii_detach: phyloc and offloc specified");

	if ((mii->mii_flags & MIIF_INITDONE) == 0)
		return;

	for (child = LIST_FIRST(&mii->mii_phys);
	     child != NULL; child = nchild) {
		nchild = LIST_NEXT(child, mii_list);
		if (phyloc != MII_PHY_ANY || offloc != MII_OFFSET_ANY) {
			if (phyloc != MII_PHY_ANY &&
			    phyloc != child->mii_phy)
				continue;
			if (offloc != MII_OFFSET_ANY &&
			    offloc != child->mii_offset)
				continue;
		}
		LIST_REMOVE(child, mii_list);
		(void) config_detach(&child->mii_dev, DETACH_FORCE);
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

int
mii_submatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct mii_attach_args *ma = aux;

	if (ma->mii_phyno != cf->cf_loc[MIICF_PHY] &&
	    cf->cf_loc[MIICF_PHY] != MIICF_PHY_DEFAULT)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
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
		rv = PHY_SERVICE(child, mii, MII_MEDIACHG);
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
		(void) PHY_SERVICE(child, mii, MII_TICK);
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
		(void) PHY_SERVICE(child, mii, MII_POLLSTAT);
}

/*
 * Inform the PHYs that the interface is down.
 */
void
mii_down(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list))
		(void) PHY_SERVICE(child, mii, MII_DOWN);
}
