/*	$NetBSD: if_ep_isa.c,v 1.1 1996/04/25 02:15:47 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
 * All rights reserved.
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/pio.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/isa/isavar.h>
#include <dev/isa/elink.h>

int ep_isa_probe __P((struct device *, void *, void *));
void ep_isa_attach __P((struct device *, struct device *, void *));

struct cfattach ep_isa_ca = {
	sizeof(struct ep_softc), ep_isa_probe, ep_isa_attach
};

static	void epaddcard __P((int, int));

#define MAXEPCARDS	10	/* 10 ISA slots */

static struct epcard {
	int	iobase;
	int	irq;
	char	available;
} epcards[MAXEPCARDS];
static int nepcards;

static void
epaddcard(iobase, irq)
	int iobase;
	int irq;
{

	if (nepcards >= MAXEPCARDS)
		return;
	epcards[nepcards].iobase = iobase;
	epcards[nepcards].irq = (irq == 2) ? 9 : irq;
	epcards[nepcards].available = 1;
	nepcards++;
}

/*
 * 3c509 cards on the ISA bus are probed in ethernet address order.
 * The probe sequence requires careful orchestration, and we'd like
 * like to allow the irq and base address to be wildcarded. So, we
 * probe all the cards the first time epprobe() is called. On subsequent
 * calls we look for matching cards.
 */
int
ep_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	static int probed;
	int slot, iobase, irq, i;
	u_short vendor, model;

	if (!probed) {
		probed = 1;

		for (slot = 0; slot < MAXEPCARDS; slot++) {
			elink_reset();
			elink_idseq(ELINK_509_POLY);

			/* Untag all the adapters so they will talk to us. */
			if (slot == 0)
				outb(ELINK_ID_PORT, TAG_ADAPTER + 0);

			vendor =
			    htons(epreadeeprom(ELINK_ID_PORT, EEPROM_MFG_ID));
			if (vendor != MFG_ID)
				continue;

			model =
			    htons(epreadeeprom(ELINK_ID_PORT, EEPROM_PROD_ID));
			if ((model & 0xfff0) != PROD_ID) {
#ifndef trusted
				printf(
				 "ep_isa_probe: ignoring model %04x\n", model);
#endif
				continue;
			}

			iobase = epreadeeprom(ELINK_ID_PORT, EEPROM_ADDR_CFG);
			iobase = (iobase & 0x1f) * 0x10 + 0x200;

			irq = epreadeeprom(ELINK_ID_PORT, EEPROM_RESOURCE_CFG);
			irq >>= 12;
			epaddcard(iobase, irq);

			/* so card will not respond to contention again */
			outb(ELINK_ID_PORT, TAG_ADAPTER + 1);

			/*
			 * XXX: this should probably not be done here
			 * because it enables the drq/irq lines from
			 * the board. Perhaps it should be done after
			 * we have checked for irq/drq collisions?
			 */
			outb(ELINK_ID_PORT, ACTIVATE_ADAPTER_TO_CONFIG);
		}
		/* XXX should we sort by ethernet address? */
	}

	for (i = 0; i < nepcards; i++) {
		if (epcards[i].available == 0)
			continue;
		if (ia->ia_iobase != IOBASEUNK &&
		    ia->ia_iobase != epcards[i].iobase)
			continue;
		if (ia->ia_irq != IRQUNK &&
		    ia->ia_irq != epcards[i].irq)
			continue;
		goto good;
	}
	return 0;

good:
	epcards[i].available = 0;
	ia->ia_iobase = epcards[i].iobase;
	ia->ia_irq = epcards[i].irq;
	ia->ia_iosize = 0x10;
	ia->ia_msize = 0;
	return 1;
}

void
ep_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	u_short conn = 0;
	int iobase;

	sc->ep_iobase = iobase = ia->ia_iobase;
	sc->bustype = EP_BUS_ISA;

	GO_WINDOW(0);
	conn = inw(iobase + EP_W0_CONFIG_CTRL);

	printf(": 3Com 3C509 Ethernet\n");

	epconfig(sc, conn);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, epintr, sc, sc->sc_dev.dv_xname);
}
