/*	$OpenBSD: rtl80x9.c,v 1.1 1998/11/06 06:34:36 fgsch Exp $	*/
/*	$NetBSD: rtl80x9.c,v 1.1 1998/10/31 00:44:33 thorpej Exp $	*/

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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#endif
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#ifdef __NetBSD__
#include <netinet/if_inarp.h>
#else
#include <netinet/if_ether.h>
#endif
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

int
rtl80x9_mediachange(dsc)
	struct dp8390_softc *dsc;
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.  The new media will be
	 * set up in ne_pci_rtl8029_init_card() called via dp8390_init().
	 */
	dp8390_reset(dsc);
	return (0);
}

void
rtl80x9_mediastatus(sc, ifmr)
	struct dp8390_softc *sc;
	struct ifmediareq *ifmr;
{
#ifdef __NetBSD__
	struct ifnet *ifp = &sc->sc_ec.ec_if;
#else
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
#endif
	u_int8_t cr_proto = sc->cr_proto |
	    ((ifp->if_flags & IFF_RUNNING) ? ED_CR_STA : ED_CR_STP);

	/*
	 * Sigh, can detect which media is being used, but can't
	 * detect if we have link or not.
	 */

	/* Set NIC to page 3 registers. */
	NIC_PUT(sc->sc_regt, sc->sc_regh, ED_P0_CR, cr_proto | ED_CR_PAGE_3);

	if (NIC_GET(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG0) &
	    RTL3_CONFIG0_BNC)
		ifmr->ifm_active = IFM_ETHER|IFM_10_2;
	else {
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (NIC_GET(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG3) &
		    RTL3_CONFIG3_FUDUP)
			ifmr->ifm_active |= IFM_FDX;
	}

	/* Set NIC to page 0 registers. */
	NIC_PUT(sc->sc_regt, sc->sc_regh, ED_P0_CR, cr_proto | ED_CR_PAGE_0);
}

void
rtl80x9_init_card(sc)
	struct dp8390_softc *sc;
{
	struct ifmedia *ifm = &sc->sc_media;
#ifdef __NetBSD__
	struct ifnet *ifp = &sc->sc_ec.ec_if;
#else
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
#endif
	u_int8_t cr_proto = sc->cr_proto |
	    ((ifp->if_flags & IFF_RUNNING) ? ED_CR_STA : ED_CR_STP);
	u_int8_t reg;

	/* Set NIC to page 3 registers. */
	NIC_PUT(sc->sc_regt, sc->sc_regh, ED_P0_CR, cr_proto | ED_CR_PAGE_3);

	/* First, set basic media type. */
	reg = NIC_GET(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG2);
	reg &= ~(RTL3_CONFIG2_PL1|RTL3_CONFIG2_PL0);
	switch (IFM_SUBTYPE(ifm->ifm_cur->ifm_media)) {
	case IFM_AUTO:
		/* Nothing to do; both bits clear == auto-detect. */
		break;

	case IFM_10_T:
		reg |= RTL3_CONFIG2_PL0;
		break;

	case IFM_10_2:
		reg |= RTL3_CONFIG2_PL1|RTL3_CONFIG2_PL0;
		break;
	}
	NIC_PUT(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG2, reg);

	/* Now, set duplex mode. */
	reg = NIC_GET(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG3);
	if (ifm->ifm_cur->ifm_media & IFM_FDX)
		reg |= RTL3_CONFIG3_FUDUP;
	else
		reg &= ~RTL3_CONFIG3_FUDUP;
	NIC_PUT(sc->sc_regt, sc->sc_regh, NERTL_RTL3_CONFIG3, reg);

	/* Set NIC to page 0 registers. */
	NIC_PUT(sc->sc_regt, sc->sc_regh, ED_P0_CR, cr_proto | ED_CR_PAGE_0);
}

void
rtl80x9_init_media(sc, mediap, nmediap, defmediap)
	struct dp8390_softc *sc;
	int **mediap, *nmediap, *defmediap;
{
	static int rtl80x9_media[] = {
		IFM_ETHER|IFM_AUTO,
		IFM_ETHER|IFM_10_T,
		IFM_ETHER|IFM_10_T|IFM_FDX,
		IFM_ETHER|IFM_10_2,
	};

	printf("%s: 10base2, 10baseT, 10baseT-FDX, auto, default auto\n",
	    sc->sc_dev.dv_xname);

	*mediap = rtl80x9_media;
	*nmediap = sizeof(rtl80x9_media) / sizeof(rtl80x9_media[0]);
	*defmediap = IFM_ETHER|IFM_AUTO;
}
