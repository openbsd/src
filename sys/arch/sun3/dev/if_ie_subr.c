/*	$NetBSD: if_ie_subr.c,v 1.7 1995/09/26 04:02:04 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
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
 *	This product includes software developed by Gordon Ross
 * 4. The name of the Author may not be used to endorse or promote products
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

/*
 * Machine-dependent glue for the Intel Ethernet (ie) driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/idprom.h>
#include <machine/vmparam.h>

#include "i82586.h"
#include "if_ie.h"
#include "if_ie_subr.h"

static void ie_obreset __P((struct ie_softc *));
static void ie_obattend __P((struct ie_softc *));
static void ie_obrun __P((struct ie_softc *));
static void ie_vmereset __P((struct ie_softc *));
static void ie_vmeattend __P((struct ie_softc *));
static void ie_vmerun __P((struct ie_softc *));

/*
 * zero/copy functions: OBIO can use the normal functions, but VME
 *    must do only byte or half-word (16 bit) accesses...
 */
static void wcopy(), wzero();

int
ie_md_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	int x, sz;

	switch (ca->ca_bustype) {

	case BUS_OBIO:
		if (ca->ca_paddr == -1)
			ca->ca_paddr = OBIO_INTEL_ETHER;
		sz = 1;
		break;

	case BUS_VME16:
		/* No default VME address. */
		if (ca->ca_paddr == -1)
			return(0);
		sz = 2;
		break;

	default:
		return (0);
	}

	/* Default interrupt level. */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 3;

	x = bus_peek(ca->ca_bustype, ca->ca_paddr, sz);
	return (x != -1);
}

void
ie_md_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct ie_softc *sc = (void *) self;
	struct confargs *ca = args;
	caddr_t mem, reg;

	/*
	 * *note*: we don't detect the difference between a VME3E and
	 * a multibus/vme card.   if you want to use a 3E you'll have
	 * to fix this.
	 */

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		sc->hard_type = IE_OBIO;
		sc->reset_586 = ie_obreset;
		sc->chan_attn = ie_obattend;
		sc->run_586 = ie_obrun;
		sc->sc_bcopy = bcopy;
		sc->sc_bzero = bzero;
		sc->sc_iobase = (caddr_t)DVMA_OBIO_SLAVE_BASE;
		sc->sc_msize = MEMSIZE;

		/* Map in the control register. */
		reg = obio_alloc(ca->ca_paddr, OBIO_INTEL_ETHER_SIZE);
		if (reg == NULL)
			panic(": not enough obio space\n");
		sc->sc_reg = reg;

		/* Allocate "shared" memory (DVMA space). */
		mem = dvma_malloc(sc->sc_msize);
		if (mem == NULL)
			panic(": not enough dvma space");
		sc->sc_maddr = mem;

		/* This is a FIXED address, in a page left by the PROM. */
		sc->scp = (volatile struct ie_sys_conf_ptr *)
		    (sc->sc_iobase + IE_SCP_ADDR);

		/* Install interrupt handler. */
		isr_add_autovect(ie_intr, (void *)sc, ca->ca_intpri);

		break;

	case BUS_VME16: {
		volatile struct ievme *iev;
		u_long  rampaddr;
		int     lcv;

		sc->hard_type = IE_VME;
		sc->reset_586 = ie_vmereset;
		sc->chan_attn = ie_vmeattend;
		sc->run_586 = ie_vmerun;
		sc->sc_bcopy = wcopy;
		sc->sc_bzero = wzero;
		sc->sc_msize = MEMSIZE;
		sc->sc_reg = bus_mapin(ca->ca_bustype, ca->ca_paddr,
							   sizeof(struct ievme));

		iev = (volatile struct ievme *) sc->sc_reg;
		/* top 12 bits */
		rampaddr = ca->ca_paddr & 0xfff00000;
		/* 4 more */
		rampaddr |= ((iev->status & IEVME_HADDR) << 16);
		sc->sc_maddr = bus_mapin(ca->ca_bustype, rampaddr, sc->sc_msize);
		sc->sc_iobase = sc->sc_maddr;
		iev->pectrl = iev->pectrl | IEVME_PARACK; /* clear to start */

		sc->scp = (volatile struct ie_sys_conf_ptr *)
		    (sc->sc_iobase + (IE_SCP_ADDR & (IEVME_PAGESIZE - 1)));

		/*
		 * set up mappings, direct map except for last page
		 * which is mapped at zero and at high address (for
		 * scp), zero ram
		 */

		for (lcv = 0; lcv < IEVME_MAPSZ - 1; lcv++)
			iev->pgmap[lcv] = IEVME_SBORDR | IEVME_OBMEM | lcv;
		iev->pgmap[IEVME_MAPSZ - 1] = IEVME_SBORDR | IEVME_OBMEM | 0;
		(sc->sc_bzero)(sc->sc_maddr, sc->sc_msize);

		isr_add_vectored(ie_intr, (void *)sc,
						 ca->ca_intpri,
						 ca->ca_intvec);
		break;
	}

	default:
		printf("unknown\n");
		return;
	}

	/*
	 * set up pointers to data structures and buffer area.
	 */
	sc->iscp = (volatile struct ie_int_sys_conf_ptr *)
	    sc->sc_maddr;
	sc->scb = (volatile struct ie_sys_ctl_block *)
	    sc->sc_maddr + sizeof(struct ie_int_sys_conf_ptr);

	/*
	 * rest of first page is unused, rest of ram
	 * for buffers
	 */
	sc->buf_area = sc->sc_maddr + IEVME_PAGESIZE;
	sc->buf_area_sz = sc->sc_msize - IEVME_PAGESIZE;

	idprom_etheraddr(sc->sc_addr); /* ethernet addr */
}


/*
 * MULTIBUS/VME support
 */
void 
ie_vmereset(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;
	iev->status = IEVME_RESET;
	delay(100);		/* XXX could be shorter? */
	iev->status = 0;
}

void 
ie_vmeattend(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;

	iev->status |= IEVME_ATTEN;	/* flag! */
	iev->status &= ~IEVME_ATTEN;	/* down. */
}

void 
ie_vmerun(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;

	iev->status |= (IEVME_ONAIR | IEVME_IENAB | IEVME_PEINT);
}

/*
 * onboard ie support
 */
void
ie_obreset(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;
	ieo->obctrl = 0;
	delay(100);			/* XXX could be shorter? */
	ieo->obctrl = IEOB_NORSET;
}
void
ie_obattend(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= IEOB_ATTEN;	/* flag! */
	ieo->obctrl &= ~IEOB_ATTEN;	/* down. */
}

void
ie_obrun(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= (IEOB_ONAIR|IEOB_IENAB|IEOB_NORSET);
}

/*
 * wcopy/wzero - like bcopy/bzero but largest access is 16-bits,
 * and also does byte swaps...
 * XXX - Would be nice to have asm versions in some library...
 */

static void
wzero(vb, l)
	void *vb;
	u_int l;
{
	u_char *b = vb;
	u_char *be = b + l;
	u_short *sp;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b & 1)
		*b++ = 0;

	/* back, */
	if (b != be && ((u_long)be & 1) != 0) {
		be--;
		*be = 0;
	}

	/* and middle. */
	sp = (u_short *)b;
	while (sp != (u_short *)be)
		*sp++ = 0;
}

static void
wcopy(vb1, vb2, l)
	const void *vb1;
	void *vb2;
	u_int l;
{
	const u_char *b1e, *b1 = vb1;
	u_char *b2 = vb2;
	u_short *sp;
	int bstore = 0;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b1 & 1) {
		*b2++ = *b1++;
		l--;
	}

	/* middle, */
	sp = (u_short *)b1;
	b1e = b1 + l;
	if (l & 1)
		b1e--;
	bstore = (u_long)b2 & 1;

	while (sp < (u_short *)b1e) {
		if (bstore) {
			b2[1] = *sp & 0xff;
			b2[0] = *sp >> 8;
		} else
			*((short *)b2) = *sp;
		sp++;
		b2 += 2;
	}

	/* and back. */
	if (l & 1)
		*b2 = *b1e;
}
