/*	$OpenBSD: if_gem_sbus.c,v 1.1 2009/07/13 19:53:58 kettenis Exp $	*/
/*	$NetBSD: if_gem_sbus.c,v 1.1 2006/11/24 13:23:32 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * SBus front-end for the GEM network driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
extern struct sparc_bus_dma_tag *iommu_dmatag;

#include <sparc/dev/sbusvar.h>

#include <dev/ic/gemreg.h>
#include <dev/ic/gemvar.h>

#include <dev/ofw/openfirm.h>

struct gem_sbus_softc {
	struct	gem_softc	gsc_gem;	/* GEM device */
	struct intrhand		gsc_ih;
	struct rom_reg		gsc_rr;
};

int	gemmatch_sbus(struct device *, void *, void *);
void	gemattach_sbus(struct device *, struct device *, void *);

struct cfattach gem_sbus_ca = {
	sizeof(struct gem_sbus_softc), gemmatch_sbus, gemattach_sbus
};

int
gemmatch_sbus(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	return (strcmp("network", ra->ra_name) == 0);
}

void
gemattach_sbus(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct gem_sbus_softc *gsc = (void *)self;
	struct gem_softc *sc = &gsc->gsc_gem;
	/* XXX the following declaration should be elsewhere */
	extern void myetheraddr(u_char *);

	/* Pass on the bus tags */
	gsc->gsc_rr = ca->ca_ra.ra_reg[0];
	sc->sc_bustag = &gsc->gsc_rr;
	sc->sc_dmatag = iommu_dmatag;

	if (ca->ca_ra.ra_nintr < 1) {
                printf(": no interrupt\n");
                return;
        }

	if (ca->ca_ra.ra_nreg < 2) {
                printf(": only %d register sets\n", ca->ca_ra.ra_nreg);
		return;
	}

	sc->sc_variant = GEM_SUN_GEM;

	/*
	 * Map two register banks:
	 *
	 *	bank 0: status, config, reset
	 *	bank 1: various gem parts
	 *
	 */
	if (bus_space_map(&ca->ca_ra.ra_reg[0], 0,
	    ca->ca_ra.ra_reg[0].rr_len, 0, &sc->sc_h2)) {
		printf(": can't map registers\n");
		return;
	}
	if (bus_space_map(&ca->ca_ra.ra_reg[1], 0,
	    ca->ca_ra.ra_reg[1].rr_len, 0, &sc->sc_h1)) {
		printf(": can't map registers\n");
		return;
	}

	if (getprop(ca->ca_ra.ra_node, "local-mac-address",
	    sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_arpcom.ac_enaddr);

	/*
	 * SBUS config
	 */
	(void) bus_space_read_4(sc->sc_bustag, sc->sc_h2, GEM_SBUS_RESET);
	delay(100);
	bus_space_write_4(sc->sc_bustag, sc->sc_h2, GEM_SBUS_CONFIG,
	    GEM_SBUS_CFG_BSIZE32);

	/* Establish interrupt handler */
	gsc->gsc_ih.ih_fun = gem_intr;
	gsc->gsc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &gsc->gsc_ih,
	    IPL_NET, self->dv_xname);

	gem_config(sc);
}
