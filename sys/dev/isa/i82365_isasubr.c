/*	$OpenBSD: i82365_isasubr.c,v 1.4 1998/12/31 09:17:44 deraadt Exp $	*/
/*	$NetBSD: i82365_isasubr.c,v 1.1 1998/06/07 18:28:31 sommerfe Exp $  */

/*
 * Copyright (c) 1998 Bill Sommerfeld.  All rights reserved.
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
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


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365var.h>
#include <dev/isa/i82365_isavar.h>

/*****************************************************************************
 * Configurable parameters.
 *****************************************************************************/

/*
 * Default I/O allocation range.  If both are set to non-zero, these
 * values will be used instead.  Otherwise, the code attempts to probe
 * the bus width.  Systems with 10 address bits should use 0x300 and 0xff.
 * Systems with 12 address bits (most) should use 0x400 and 0xbff.
 */

#ifndef PCIC_ISA_ALLOC_IOBASE
#define	PCIC_ISA_ALLOC_IOBASE		0
#endif

#ifndef PCIC_ISA_ALLOC_IOSIZE
#define	PCIC_ISA_ALLOC_IOSIZE		0
#endif

int	pcic_isa_alloc_iobase = PCIC_ISA_ALLOC_IOBASE;
int	pcic_isa_alloc_iosize = PCIC_ISA_ALLOC_IOSIZE;


/*
 * Default IRQ allocation bitmask.  This defines the range of allowable
 * IRQs for PCMCIA slots.  Useful if order of probing would screw up other
 * devices, or if PCIC hardware/cards have trouble with certain interrupt
 * lines.
 */

char	pcic_isa_intr_list[] = {
	3, 4, 12, 14, 9, 5, 10, 11, 15, 13, 7
};

int	npcic_isa_intr_list =
	sizeof(pcic_isa_intr_list) / sizeof(pcic_isa_intr_list[0]);

/*****************************************************************************
 * End of configurable parameters.
 *****************************************************************************/

#ifdef PCICISADEBUG
int	pcicsubr_debug = 1 /* XXX */ ;
#define	DPRINTF(arg) if (pcicsubr_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

void pcic_isa_bus_width_probe (sc, iot, ioh, base, length)
	struct pcic_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t base;
	u_int32_t length;
{
	bus_space_handle_t ioh_high;
	int i, iobuswidth, tmp1, tmp2;

	/*
	 * figure out how wide the isa bus is.  Do this by checking if the
	 * pcic controller is mirrored 0x400 above where we expect it to be.
	 */

	iobuswidth = 12;

	/* Map i/o space. */
	if (bus_space_map(iot, base + 0x400, length, 0, &ioh_high)) {
		printf("%s: can't map high i/o space\n", sc->dev.dv_xname);
		return;
	}

	for (i = 0; i < PCIC_NSLOTS; i++) {
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP) {
			/*
			 * read the ident flags from the normal space and
			 * from the mirror, and compare them
			 */

			bus_space_write_1(iot, ioh, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp1 = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

			bus_space_write_1(iot, ioh_high, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp2 = bus_space_read_1(iot, ioh_high, PCIC_REG_DATA);

			if (tmp1 == tmp2)
				iobuswidth = 10;
		}
	}

	bus_space_free(iot, ioh_high, length);

	/*
	 * XXX mycroft recommends I/O space range 0x400-0xfff .  I should put
	 * this in a header somewhere
	 */

	/*
	 * XXX some hardware doesn't seem to grok addresses in 0x400 range--
	 * apparently missing a bit or more of address lines. (e.g.
	 * CIRRUS_PD672X with Linksys EthernetCard ne2000 clone in TI
	 * TravelMate 5000--not clear which is at fault)
	 * 
	 * Add a kludge to detect 10 bit wide buses and deal with them,
	 * and also a config file option to override the probe.
	 */

	if (iobuswidth == 10) {
		sc->iobase = 0x300;
		sc->iosize = 0x0ff;
	} else {
#if 0
		/*
		 * This is what we'd like to use, but...
		 */
		sc->iobase = 0x400;
		sc->iosize = 0xbff;
#else
		/*
		 * ...the above bus width probe doesn't always work.
		 * So, experimentation has shown the following range
		 * to not lose on systems that 0x300-0x3ff loses on
		 * (e.g. the NEC Versa 6030X).
		 */
		sc->iobase = 0x330;
		sc->iosize = 0x0cf;
#endif
	}

	DPRINTF(("%s: bus_space_alloc range 0x%04lx-0x%04lx (probed)\n",
	    sc->dev.dv_xname, (long) sc->iobase,

	    (long) sc->iobase + sc->iosize));

	if (pcic_isa_alloc_iobase && pcic_isa_alloc_iosize) {
		sc->iobase = pcic_isa_alloc_iobase;
		sc->iosize = pcic_isa_alloc_iosize;

		DPRINTF(("%s: bus_space_alloc range 0x%04lx-0x%04lx "
		    "(config override)\n", sc->dev.dv_xname, (long) sc->iobase,
		    (long) sc->iobase + sc->iosize));
	}
}


void *
pcic_isa_chip_intr_establish(pch, pf, ipl, fct, arg)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_function *pf;
	int ipl;
	int (*fct) __P((void *));
	void *arg;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	isa_chipset_tag_t ic = h->sc->intr_est;
	int irq, ist;
	void *ih;
	int i, reg;

	if (pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)
		ist = IST_LEVEL;
	else if (pf->cfe->flags & PCMCIA_CFE_IRQPULSE)
		ist = IST_PULSE;
	else
		ist = IST_LEVEL;

	for (i = 0; i < npcic_isa_intr_list; i++)
		if (isa_intr_check(ic, pcic_isa_intr_list[i], ist) == 2)
			goto found;
	for (i = 0; i < npcic_isa_intr_list; i++)
		if (isa_intr_check(ic, pcic_isa_intr_list[i], ist) == 1)
			goto found;
	return (NULL);

found:
	irq = pcic_isa_intr_list[i];
	if ((ih = isa_intr_establish(ic, irq, ist, ipl,
	    fct, arg, h->pcmcia->dv_xname)) == NULL)
		return (NULL);

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~PCIC_INTR_IRQ_MASK;
	reg |= irq;
	pcic_write(h, PCIC_INTR, reg);

	h->ih_irq = irq;

	printf("%s: card irq %d\n", h->pcmcia->dv_xname, irq);

	return (ih);
}

void 
pcic_isa_chip_intr_disestablish(pch, ih)
	pcmcia_chipset_handle_t pch;
	void *ih;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	isa_chipset_tag_t ic = h->sc->intr_est;
	int reg;

	h->ih_irq = 0;

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~(PCIC_INTR_IRQ_MASK | PCIC_INTR_ENABLE);
	pcic_write(h, PCIC_INTR, reg);

	isa_intr_disestablish(ic, ih);
}
