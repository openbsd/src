/*	$NetBSD: if_le.c,v 1.1 1995/12/20 00:52:16 cgd Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#ifdef alpha
#define	CAN_HAVE_IOASIC	1
#define	CAN_HAVE_TC	1
#endif
#ifdef pmax
/* XXX PMAX BASEBOARD OPTIONS? */
#define	CAN_HAVE_IOASIC	1
#define	CAN_HAVE_TC	1
#endif

#include "bpfilter.h"
/* XXX PMAX BASEBOARD OPTIONS? */
#ifdef CAN_HAVE_TC
#include "tc.h"
#endif
#ifdef CAN_HAVE_IOASIC
#include "ioasic.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>

/* XXX PMAX BASEBOARD OPTIONS? */
#if CAN_HAVE_TC && (NTC > 0)
#include <dev/tc/tcvar.h>
#endif
#if CAN_HAVE_IOASIC && (NIOASIC > 0)
#include <dev/tc/ioasicvar.h>
#endif

#include <dev/tc/if_levar.h>
#include <dev/ic/am7990reg.h>
#define	LE_NEED_BUF_CONTIG
#define	LE_NEED_BUF_GAP2
#define	LE_NEED_BUF_GAP16
#include <dev/ic/am7990var.h>

/* access LANCE registers */
void lewritereg();
#define	LERDWR(cntl, src, dst)	{ (dst) = (src); tc_mb(); }
#define	LEWREG(src, dst)	lewritereg(&(dst), (src))

#define LE_OFFSET_RAM		0x0
#define LE_OFFSET_LANCE		0x100000
#define LE_OFFSET_ROM		0x1c0000

extern caddr_t le_iomem;

#define	LE_SOFTC(unit)	lecd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

int lematch __P((struct device *, void *, void *));
void leattach __P((struct device *, struct device *, void *));
int leintr __P((void *));

struct cfdriver lecd = {
	NULL, "le", lematch, leattach, DV_IFNET, sizeof (struct le_softc)
};

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = sc->sc_r1;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(port, val, ler1->ler1_rdp);
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = sc->sc_r1;
	u_int16_t val;

	LEWREG(port, ler1->ler1_rap);
	LERDWR(0, ler1->ler1_rdp, val);
	return (val);
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	/* XXX VARIOUS PMAX BASEBOARD CASES? */
#if CAN_HAVE_IOASIC && (NIOASIC > 0)
	if (parent->dv_cfdata->cf_driver == &ioasiccd) {
		struct ioasicdev_attach_args *d = aux;

		if (!ioasic_submatch(match, aux))
			return (0);
		if (strncmp("lance   ", d->iada_modname, TC_ROM_LLEN))
			return (0);
	} else
#endif /* IOASIC */
#if CAN_HAVE_TC && (NTC > 0)
	if (parent->dv_cfdata->cf_driver == &tccd) {
		struct tcdev_attach_args *d = aux;

		if (!tc_submatch(match, aux))
			return (0);
		if (strncmp("PMAD-AA ", d->tcda_modname, TC_ROM_LLEN) &&
		    strncmp("PMAD-BA ", d->tcda_modname, TC_ROM_LLEN))
			return (0);
	} else
#endif /* TC */
		return (0);

	return (1);
}

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *sc = (void *)self;
	void (*ie_fn) __P((struct device *, void *, tc_intrlevel_t,
	    int (*)(void *), void *));
	u_char *cp;	/* pointer to MAC address */
	int i;

	/* XXX VARIOUS PMAX BASEBOARD CASES? */
#if CAN_HAVE_IOASIC && (NIOASIC > 0)
	if (parent->dv_cfdata->cf_driver == &ioasiccd) {
		struct ioasicdev_attach_args *d = aux;

		/* It's on the system IOCTL ASIC */
		sc->sc_r1 = (struct lereg1 *)
			TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->iada_addr));
		sc->sc_mem = (void *)TC_PHYS_TO_UNCACHED(le_iomem);
		cp = ioasic_lance_ether_address();

		sc->sc_copytodesc = copytobuf_gap2;
		sc->sc_copyfromdesc = copyfrombuf_gap2;
		sc->sc_copytobuf = copytobuf_gap16;
		sc->sc_copyfrombuf = copyfrombuf_gap16;
		sc->sc_zerobuf = zerobuf_gap16;

		ioasic_lance_dma_setup(le_iomem);	/* XXX more thought */
		ie_fn = ioasic_intr_establish;
	} else
#endif /* IOASIC */
#if CAN_HAVE_TC && (NTC > 0)			/* XXX KN02 BASEBOARD CASE? */
	if (parent->dv_cfdata->cf_driver == &tccd) {
		struct tcdev_attach_args *d = aux;

		/*
		 * It's on the turbochannel proper.
		 */
		sc->sc_r1 = (struct lereg1 *)(d->tcda_addr + LE_OFFSET_LANCE);
		sc->sc_mem = (void *)(d->tcda_addr + LE_OFFSET_RAM);
		cp = (u_char *)(d->tcda_addr + LE_OFFSET_ROM + 2);

		sc->sc_copytodesc = copytobuf_contig;
		sc->sc_copyfromdesc = copyfrombuf_contig;
		sc->sc_copytobuf = copytobuf_contig;
		sc->sc_copyfrombuf = copyfrombuf_contig;
		sc->sc_zerobuf = zerobuf_contig;

		/* XXX DMA setup fn? */
		ie_fn = tc_intr_establish;
	} else
#endif /* TC */
		panic("leattach: can't be here");

	sc->sc_conf3 = 0;
	sc->sc_addr = 0;
	sc->sc_memsize = 65536;

	/*
	 * Get the ethernet address out of rom
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++) {
		sc->sc_arpcom.ac_enaddr[i] = *cp;
		cp += 4;
	}

	sc->sc_arpcom.ac_if.if_name = lecd.cd_name;
	leconfig(sc);

	(*ie_fn)(parent, sc->sc_cookie, TC_IPL_NET, leintr, sc);
}

/*
 * Write a lance register port, reading it back to ensure success. This seems
 * to be necessary during initialization, since the chip appears to be a bit
 * pokey sometimes.
 */
void
lewritereg(regptr, val)
	register volatile u_short *regptr;
	register u_short val;
{
	register int i = 0;

	while (*regptr != val) {
		*regptr = val;
		tc_mb();
		if (++i > 10000) {
			printf("le: Reg did not settle (to x%x): x%x\n", val,
			    *regptr);
			return;
		}
		DELAY(100);
	}
}

/*
 * Routines for accessing the transmit and receive buffers are provided
 * by am7990.c, because of the LE_NEED_BUF_* macros defined above.
 * Unfortunately, CPU addressing of these buffers is done in one of
 * 3 ways:
 * - contiguous (for the 3max and turbochannel option card)
 * - gap2, which means shorts (2 bytes) interspersed with short (2 byte)
 *   spaces (for the pmax)
 * - gap16, which means 16bytes interspersed with 16byte spaces
 *   for buffers which must begin on a 32byte boundary (for 3min, maxine,
 *   and alpha)
 * The buffer offset is the logical byte offset, assuming contiguous storage.
 */

#include <dev/ic/am7990.c>
