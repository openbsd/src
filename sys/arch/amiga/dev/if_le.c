/*	$NetBSD: if_le.c,v 1.16 1995/12/27 07:09:37 chopps Exp $	*/

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

#include "bpfilter.h"

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

#include <machine/cpu.h>
#include <machine/mtpr.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_levar.h>
#include <dev/ic/am7990reg.h>
#define LE_NEED_BUF_CONTIG
#include <dev/ic/am7990var.h>

/* offsets for:	   ID,   REGS,    MEM */
int	lestd[] = { 0, 0x4000, 0x8000 };

#define	LE_SOFTC(unit)	lecd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

int lematch __P((struct device *, void *, void *));
void leattach __P((struct device *, struct device *, void *));
int leintr __P((void *));

struct cfdriver lecd = {
	NULL, "le", lematch, leattach, DV_IFNET, sizeof(struct le_softc)
};

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = sc->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = sc->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/* Commodore ethernet card */
	if (zap->manid == 514 && zap->prodid == 112)
		return (1);

	/* Ameristar ethernet card */
	if (zap->manid == 1053 && zap->prodid == 1)
		return (1);

	return (0);
}

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *sc = (void *)self;
	struct zbus_args *zap = aux;
	char *cp;
	int i;
	u_long ser;

	sc->sc_r1 = (struct lereg1 *)(lestd[1] + (int)zap->va);
	sc->sc_mem = (void *)(lestd[2] + (int)zap->va);

	sc->sc_copytodesc = copytobuf_contig;
	sc->sc_copyfromdesc = copyfrombuf_contig;
	sc->sc_copytobuf = copytobuf_contig;
	sc->sc_copyfrombuf = copyfrombuf_contig;
	sc->sc_zerobuf = zerobuf_contig;

	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_addr = 0x8000;

	/*
	 * Manufacturer decides the 3 first bytes, i.e. ethernet vendor ID.
	 */
	switch (zap->manid) {
	case 514:
		/* Commodore */
		sc->sc_memsize = 32768;
		sc->sc_arpcom.ac_enaddr[0] = 0x00;
		sc->sc_arpcom.ac_enaddr[1] = 0x80;
		sc->sc_arpcom.ac_enaddr[2] = 0x10;
		break;

	case 1053:
		/* Ameristar */
		sc->sc_memsize = 32768;
		sc->sc_arpcom.ac_enaddr[0] = 0x00;
		sc->sc_arpcom.ac_enaddr[1] = 0x00;
		sc->sc_arpcom.ac_enaddr[2] = 0x9f;
		break;

	default:
		panic("leattach: bad manid");
	}

	/*
	 * Serial number for board is used as host ID.
	 */
	ser = (u_long)zap->serno;
	sc->sc_arpcom.ac_enaddr[3] = (ser >> 16) & 0xff;
	sc->sc_arpcom.ac_enaddr[4] = (ser >>  8) & 0xff;
	sc->sc_arpcom.ac_enaddr[5] = (ser      ) & 0xff;

	sc->sc_arpcom.ac_if.if_name = lecd.cd_name;
	leconfig(sc);

	sc->sc_isr.isr_intr = leintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_isr);
}

#include <dev/ic/am7990.c>
