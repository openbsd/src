/*	$NetBSD: if_le.c,v 1.24 1995/12/11 12:43:28 pk Exp $	*/

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
#include <sys/malloc.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/if_lereg.h>
#include <sparc/dev/if_levar.h>
#include <dev/ic/am7990reg.h>
#define	LE_NEED_BUF_CONTIG
#include <dev/ic/am7990var.h>

#define	LE_SOFTC(unit)	lecd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

int	lematch __P((struct device *, void *, void *));
void	leattach __P((struct device *, struct device *, void *));
int	leintr __P((void *));

struct	cfdriver lecd = {
	NULL, "le", lematch, leattach, DV_IFNET, sizeof(struct le_softc)
};

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	register struct lereg1 *ler1 = sc->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	register struct lereg1 *ler1 = sc->sc_r1;
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
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	return (probeget(ra->ra_vaddr, 2) != -1);
}

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *sc = (void *)self;
	struct confargs *ca = aux;
	int pri;
	struct bootpath *bp;
	u_long laddr;
	int dmachild = strncmp(parent->dv_xname, "ledma", 5) == 0;

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);

	sc->sc_r1 = (struct lereg1 *)mapiodev(ca->ca_ra.ra_reg, 0,
					      sizeof(struct lereg1),
					      ca->ca_bustype);
	sc->sc_conf3 = LE_C3_BSWP | LE_C3_ACON | LE_C3_BCON;
	laddr = (u_long)dvma_malloc(MEMSIZE, &sc->sc_mem, M_NOWAIT);
#if defined (SUN4M)
	if ((laddr & 0xffffff) >= (laddr & 0xffffff) + MEMSIZE) 
		panic("if_le: Lance buffer crosses 16MB boundary");
#endif
	sc->sc_addr = laddr & 0xffffff;
	sc->sc_memsize = MEMSIZE;

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = copytobuf_contig;
	sc->sc_copyfromdesc = copyfrombuf_contig;
	sc->sc_copytobuf = copytobuf_contig;
	sc->sc_copyfrombuf = copyfrombuf_contig;
	sc->sc_zerobuf = zerobuf_contig;

	sc->sc_arpcom.ac_if.if_name = lecd.cd_name;
	leconfig(sc);

	bp = ca->ca_ra.ra_bp;
	switch (ca->ca_bustype) {
#if defined(SUN4C) || defined(SUN4M)
#define SAME_LANCE(bp, ca) \
	((bp->val[0] == ca->ca_slot && bp->val[1] == ca->ca_offset) || \
	 (bp->val[0] == -1 && bp->val[1] == sc->sc_dev.dv_unit))

	case BUS_SBUS:
		sc->sc_sd.sd_reset = (void *)lereset;
		if (dmachild) {
#ifdef notyet
			sc->sc_dma = (struct dma_softc *)parent;
			sc->sc_dma->sc_le = sc;
			sc->sc_dma->sc_regs->en_bar = laddr & 0xff000000;
			sbus_establish(&sc->sc_sd, parent);
#endif
		} else {
			sc->sc_dma = NULL;
			sbus_establish(&sc->sc_sd, &sc->sc_dev);
		}

		if (bp != NULL && strcmp(bp->name, lecd.cd_name) == 0 &&
		    SAME_LANCE(bp, ca))
			bootdv = &sc->sc_dev;
		break;
#endif /* SUN4C || SUN4M */

	default:
		if (bp != NULL && strcmp(bp->name, lecd.cd_name) == 0 &&
		    sc->sc_dev.dv_unit == bp->val[1])
			bootdv = &sc->sc_dev;
		break;
	}

	sc->sc_ih.ih_fun = leintr;
#if defined(SUN4M) && 0
	if (cputyp == CPU_SUN4M)
		sc->sc_ih.ih_fun = myleintr;
#endif
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih);

	/* now initialize DMA */
	if (sc->sc_dma) {
		dmaenintr(sc->sc_dma);
	}
}

#include <dev/ic/am7990.c>
