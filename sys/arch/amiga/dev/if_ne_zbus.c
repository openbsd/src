/*	$OpenBSD: if_ne_zbus.c,v 1.1 2000/02/29 19:05:22 niklas Exp $	*/
/*	$NetBSD: if_ne_zbus.c,v 1.5 2000/01/23 21:06:13 aymeric Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bernd Ernesti.
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
 * Thanks to Village Tronic for giving me a card.
 * Bernd Ernesti
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_media.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#else
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>
 
#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>

#include <amiga/dev/zbusvar.h>

#ifdef __NetBSD__
int	ne_zbus_match __P((struct device *, struct cfdata *, void *));
#else
int	ne_zbus_match __P((struct device *, void *, void *));
#endif
void	ne_zbus_attach __P((struct device *, struct device *, void *));
int	ne_zbus_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *));
int	ne_zbus_unmap __P((bus_space_tag_t, bus_space_handle_t, bus_size_t));

struct ne_zbus_softc {
	struct ne2000_softc	sc_ne2000;
#ifdef __NetBSD__
	struct bus_space_tag	sc_bst;
#else
	struct amiga_bus_space	sc_bst;
#endif
	struct isr		sc_isr;
};

struct cfattach ne_zbus_ca = {
	sizeof(struct ne_zbus_softc), ne_zbus_match, ne_zbus_attach
};

/*
 * The Amiga address are shifted by one bit to the ISA-Bus, but 
 * this is handled by the bus_space functions.
 */
#define	NE_ARIADNE_II_NPORTS	0x20
#define	NE_ARIADNE_II_NICBASE	0x0300	/* 0x0600 */
#define	NE_ARIADNE_II_NICSIZE	0x10
#define	NE_ARIADNE_II_ASICBASE	0x0310	/* 0x0620 */
#define	NE_ARIADNE_II_ASICSIZE	0x10

int
ne_zbus_match(parent, cf, aux)
	struct device *parent;
#ifdef __NetBSD__
	struct cfdata *cf;
#else
	void *cf;
#endif
	void *aux;
{
	struct zbus_args *zap = aux;

	/* Ariadne II ethernet card */
	if (zap->manid == 2167 && zap->prodid == 202)
		return (1);

#ifdef __NetBSD__
	/* X-serv ethernet card */
	if (zap->manid == 4626 && zap->prodid == 23)
		return (1);
#endif

	return (0);
}

/*
 * Install interface into kernel networking data structures
 */
void
ne_zbus_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct ne_zbus_softc *zsc = (struct ne_zbus_softc *)self;
	struct ne2000_softc *nsc = &zsc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct zbus_args *zap = aux;
	bus_space_tag_t nict = &zsc->sc_bst;
	bus_space_handle_t nich;
	bus_space_tag_t asict = nict;
	bus_space_handle_t asich;
	int *media, nmedia, defmedia;

	media = NULL;
	nmedia = defmedia = 0;

	dsc->sc_mediachange = rtl80x9_mediachange;
	dsc->sc_mediastatus = rtl80x9_mediastatus;
	dsc->init_card = rtl80x9_init_card;

#ifdef __NetBSD__
	zsc->sc_bst.base = (u_long)zap->va + 0;
	if (zap->manid == 4626)
		 zsc->sc_bst.base += 0x8000;

	zsc->sc_bst.absm = &amiga_bus_stride_2;
#else
	zsc->sc_bst.bs_data = zap->va + (zap->manid == 4626 ? 0x8000 : 0);
	zsc->sc_bst.bs_map = ne_zbus_map;
	zsc->sc_bst.bs_unmap = ne_zbus_unmap;
	zsc->sc_bst.bs_swapped = 1;
	zsc->sc_bst.bs_shift = 1;
#endif

	printf("\n");

	/* Map i/o space. */
	if (bus_space_map(nict, NE_ARIADNE_II_NICBASE, NE_ARIADNE_II_NPORTS, 0,
	    &nich)) {
		printf("%s: can't map nic i/o space\n", dsc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_subregion(nict, nich, NE2000_ASIC_OFFSET,
	    NE_ARIADNE_II_ASICSIZE, &asich)) {
		printf("%s: can't map asic i/o space\n", dsc->sc_dev.dv_xname);
		return; 
	}

	dsc->sc_regt = nict;
	dsc->sc_regh = nich;

	nsc->sc_asict = asict;
	nsc->sc_asich = asich;

	/* Initialize media. */
	rtl80x9_init_media(dsc, &media, &nmedia, &defmedia);

	/* This interface is always enabled. */
	dsc->sc_enabled = 1;

	/*
	 * Do generic NE2000 attach.  This will read the station address
	 * from the EEPROM.
	 */
	ne2000_attach(nsc, NULL, media, nmedia, defmedia);

	zsc->sc_isr.isr_intr = dp8390_intr;
	zsc->sc_isr.isr_arg = dsc;
	zsc->sc_isr.isr_ipl = 2;
	add_isr(&zsc->sc_isr);
}

int
ne_zbus_map(bst, addr, sz, cacheable, handle)
	bus_space_tag_t bst;
	bus_addr_t addr;
	bus_size_t sz;
	int cacheable;
	bus_space_handle_t *handle;
{
	*handle = (bus_space_handle_t)((bus_addr_t)bst->bs_data + 2 * addr);
	return (0);
}

int
ne_zbus_unmap(bst, handle, sz)
	bus_space_tag_t bst;
	bus_space_handle_t handle;
	bus_size_t sz;
{
	return (0);
}
