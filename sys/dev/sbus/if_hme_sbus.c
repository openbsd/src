/*	$OpenBSD: if_hme_sbus.c,v 1.4 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: if_hme_sbus.c,v 1.6 2001/02/28 14:52:48 mrg Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * SBus front-end device driver for the HME ethernet device.
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

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>
#include <dev/ic/hmevar.h>
#include <dev/ofw/openfirm.h>

struct hmesbus_softc {
	struct	hme_softc	hsc_hme;	/* HME device */
	struct	sbusdev		hsc_sbus;	/* SBus device */
};

int	hmematch_sbus(struct device *, void *, void *);
void	hmeattach_sbus(struct device *, struct device *, void *);

struct cfattach hme_sbus_ca = {
	sizeof(struct hmesbus_softc), hmematch_sbus, hmeattach_sbus
};

int
hmematch_sbus(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct cfdata *cf = vcf;
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0 ||
	    strcmp("SUNW,qfe", sa->sa_name) == 0 ||
	    strcmp("SUNW,hme", sa->sa_name) == 0);
}

void
hmeattach_sbus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct hmesbus_softc *hsc = (void *)self;
	struct hme_softc *sc = &hsc->hsc_hme;
	struct sbusdev *sd = &hsc->hsc_sbus;
	u_int32_t burst, sbusburst;
	int node;
	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);

	node = sa->sa_node;

	/* Pass on the bus tags */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	if (sa->sa_nreg < 5) {
		printf("%s: only %d register sets\n",
			self->dv_xname, sa->sa_nreg);
		return;
	}

	/*
	 * Map five register banks:
	 *
	 *	bank 0: HME SEB registers
	 *	bank 1: HME ETX registers
	 *	bank 2: HME ERX registers
	 *	bank 3: HME MAC registers
	 *	bank 4: HME MIF registers
	 *
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[0].sbr_slot,
			 (bus_addr_t)sa->sa_reg[0].sbr_offset,
			 (bus_size_t)sa->sa_reg[0].sbr_size,
			 BUS_SPACE_MAP_LINEAR, 0, &sc->sc_seb) != 0) {
		printf("%s @ sbus: cannot map registers\n", self->dv_xname);
		return;
	}
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[1].sbr_slot,
			 (bus_addr_t)sa->sa_reg[1].sbr_offset,
			 (bus_size_t)sa->sa_reg[1].sbr_size,
			 BUS_SPACE_MAP_LINEAR, 0, &sc->sc_etx) != 0) {
		printf("%s @ sbus: cannot map registers\n", self->dv_xname);
		return;
	}
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[2].sbr_slot,
			 (bus_addr_t)sa->sa_reg[2].sbr_offset,
			 (bus_size_t)sa->sa_reg[2].sbr_size,
			 BUS_SPACE_MAP_LINEAR, 0, &sc->sc_erx) != 0) {
		printf("%s @ sbus: cannot map registers\n", self->dv_xname);
		return;
	}
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[3].sbr_slot,
			 (bus_addr_t)sa->sa_reg[3].sbr_offset,
			 (bus_size_t)sa->sa_reg[3].sbr_size,
			 BUS_SPACE_MAP_LINEAR, 0, &sc->sc_mac) != 0) {
		printf("%s @ sbus: cannot map registers\n", self->dv_xname);
		return;
	}
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[4].sbr_slot,
			 (bus_addr_t)sa->sa_reg[4].sbr_offset,
			 (bus_size_t)sa->sa_reg[4].sbr_size,
			 BUS_SPACE_MAP_LINEAR, 0, &sc->sc_mif) != 0) {
		printf("%s @ sbus: cannot map registers\n", self->dv_xname);
		return;
	}

	sd->sd_reset = (void *)hme_reset;
	sbus_establish(sd, self);

	if (OF_getprop(sa->sa_node, "local-mac-address",
	    sc->sc_enaddr, ETHER_ADDR_LEN) <= 0)
		myetheraddr(sc->sc_enaddr);

	/*
	 * Get transfer burst size from PROM and pass it on
	 * to the back-end driver.
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	burst = getpropint(node, "burst-sizes", -1);
	if (burst == -1)
		/* take SBus burst sizes */
		burst = sbusburst;

	/* Clamp at parent's burst sizes */
	burst &= sbusburst;

	/* Translate into plain numerical format */
	sc->sc_burst =  (burst & SBUS_BURST_32) ? 32 :
			(burst & SBUS_BURST_16) ? 16 : 0;

	sc->sc_pci = 0; /* XXXXX should all be done in bus_dma. */
	hme_config(sc);

	/* Establish interrupt handler */
	if (sa->sa_nintr != 0)
		(void)bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_NET, 0,
					 hme_intr, sc);
}
