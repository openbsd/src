/*	$NetBSD: if_ie_vmes.c,v 1.5 1996/11/20 18:56:51 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include <machine/idprom.h>
#include <machine/vmparam.h>

#include "i82586.h"
#include "if_iereg.h"
#include "if_ievar.h"

static void ie_vmereset __P((struct ie_softc *));
static void ie_vmeattend __P((struct ie_softc *));
static void ie_vmerun __P((struct ie_softc *));

/*
 * zero/copy functions: OBIO can use the normal functions, but VME
 *    must do only byte or half-word (16 bit) accesses...
 */
static void wcopy(), wzero();

/*
 * New-style autoconfig attachment
 */

static int  ie_vmes_match __P((struct device *, void *, void *));
static void ie_vmes_attach __P((struct device *, struct device *, void *));

struct cfattach ie_vmes_ca = {
	sizeof(struct ie_softc), ie_vmes_match, ie_vmes_attach
};


static int
ie_vmes_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	int x, sz;

#ifdef	DIAGNOSTIC
	if (ca->ca_bustype != BUS_VME16) {
		printf("ie_vmes_match: bustype %d?\n", ca->ca_bustype);
		return (0);
	}
#endif

	/* No default VME address. */
	if (ca->ca_paddr == -1)
		return(0);

	/* Default interrupt level. */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 3;

	x = bus_peek(ca->ca_bustype, ca->ca_paddr, 2);
	return (x != -1);
}

/*
 * *note*: we don't detect the difference between a VME3E and
 * a multibus/vme card.   if you want to use a 3E you'll have
 * to fix this.
 */
void
ie_vmes_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct ie_softc *sc = (void *) self;
	struct confargs *ca = args;
	volatile struct ievme *iev;
	u_long  rampaddr;
	int     lcv, off;

	sc->hard_type = IE_VME;
	sc->reset_586 = ie_vmereset;
	sc->chan_attn = ie_vmeattend;
	sc->run_586 = ie_vmerun;
	sc->sc_bcopy = wcopy;
	sc->sc_bzero = wzero;

	/*
	 * There is 64K of memory on the VME board.
	 * (determined by hardware - NOT configurable!)
	 */
	sc->sc_msize = 0x10000; /* MEMSIZE 64K */

	/* Map in the board control regs. */
	sc->sc_reg = bus_mapin(ca->ca_bustype, ca->ca_paddr,
						   sizeof(struct ievme));
	iev = (volatile struct ievme *) sc->sc_reg;

	/*
	 * Find and map in the board memory.
	 */
	/* top 12 bits */
	rampaddr = ca->ca_paddr & 0xfff00000;
	/* 4 more */
	rampaddr |= ((iev->status & IEVME_HADDR) << 16);
	sc->sc_maddr = bus_mapin(ca->ca_bustype, rampaddr, sc->sc_msize);

	/*
	 * On this hardware, the i82586 address is just
	 * masked to 16 bits, so sc_iobase == sc_maddr
	 */
	sc->sc_iobase = sc->sc_maddr;

	/*
	 * Set up on-board mapping registers for linear map.
	 */
	iev->pectrl |= IEVME_PARACK; /* clear to start */
	for (lcv = 0; lcv < IEVME_MAPSZ; lcv++)
		iev->pgmap[lcv] = IEVME_SBORDR | IEVME_OBMEM | lcv;
	(sc->sc_bzero)(sc->sc_maddr, sc->sc_msize);

	/*
	 * Set the System Configuration Pointer (SCP).
	 * Its location is system-dependent because the
	 * i82586 reads it from a fixed physical address.
	 * On this hardware, the i82586 address is just
	 * masked down to 16 bits, so the SCP is found
	 * at the end of the RAM on the VME board.
	 */
	off = IE_SCP_ADDR & 0xFFFF;
	sc->scp = (volatile void *) (sc->sc_maddr + off);

	/*
	 * The rest of ram is used for buffers, etc.
	 */
	sc->buf_area = sc->sc_maddr;
	sc->buf_area_sz = off;

	/* Set the ethernet address. */
	idprom_etheraddr(sc->sc_addr);

	/* Do machine-independent parts of attach. */
	ie_attach(sc);

	/* Install interrupt handler. */
	isr_add_vectored(ie_intr, (void *)sc,
		ca->ca_intpri, ca->ca_intvec);
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
