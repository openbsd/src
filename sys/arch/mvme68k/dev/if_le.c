/*	$OpenBSD: if_le.c,v 1.12 1998/09/16 22:41:18 jason Exp $ */

/*-
 * Copyright (c) 1982, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_le.c	8.2 (Berkeley) 10/30/93
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

#include <net/if_media.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <mvme68k/dev/if_lereg.h>
#include <mvme68k/dev/if_levar.h>
#include <mvme68k/dev/pccreg.h>

/* autoconfiguration driver */
void	leattach(struct device *, struct device *, void *);
int	lematch(struct device *, void *, void *);

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

hide void lewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t lerdcsr __P((struct am7990_softc *, u_int16_t));

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	register struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	register struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
lematch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;

	return (!badvaddr(ca->ca_vaddr, 2));
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
leattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct confargs *ca = aux;
	register int a;
	int pri = ca->ca_ipl;
	extern void *etherbuf;

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);

	/* Are we the boot device? */
	if (ca->ca_paddr == bootaddr)
		bootdv = self;

	sc->sc_mem = etherbuf;
	lesc->sc_r1 = (struct lereg1 *)ca->ca_vaddr;
	sc->sc_conf3 = LE_C3_BSWP /*| LE_C3_ACON | LE_C3_BCON*/;
	sc->sc_addr = kvtop(sc->sc_mem);
	sc->sc_memsize = LEMEMSIZE;

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	evcnt_attach(&sc->sc_dev, "intr", &lesc->sc_intrcnt);
	evcnt_attach(&sc->sc_dev, "errs", &lesc->sc_errcnt);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwreset = NULL;
	sc->sc_hwinit = NULL;

	am7990_config(sc);

	/* connect the interrupt */
	lesc->sc_ih.ih_fn = am7990_intr;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_ipl = pri;
	pccintr_establish(PCCV_LE, &lesc->sc_ih);
	((struct pccreg *)ca->ca_master)->pcc_leirq = pri | PCC_IRQ_IEN;
}
