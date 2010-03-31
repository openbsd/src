/*	$OpenBSD: if_le_syscon.c,v 1.8 2010/03/31 19:46:27 miod Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <uvm/uvm.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/cpu.h>

#include <aviion/dev/sysconvar.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

/*
 * LANCE registers. Although these are 16 bit registers, on the AV400
 * design, they need to be accessed as 32 bit registers. Bus magic...
 * The real stuff is in dev/ic/am7990reg.h
 */
struct av_lereg {
	volatile u_int32_t	ler1_rdp;	/* data port */
	volatile u_int32_t	ler1_rap;	/* register select port */
};

/*
 * Ethernet software status per interface.
 * The real stuff is in dev/ic/am7990var.h
 */
struct	le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	struct	av_lereg *sc_r1;	/* LANCE registers */
	struct	intrhand sc_ih;
};

int	le_syscon_match(struct device *, void *, void *);
void	le_syscon_attach(struct device *, struct device *, void *);

struct cfattach le_syscon_ca = {
	sizeof(struct le_softc), le_syscon_match, le_syscon_attach
};

void	le_syscon_wrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
u_int16_t le_syscon_rdcsr(struct am7990_softc *, u_int16_t);

void
le_syscon_wrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

u_int16_t
le_syscon_rdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
le_syscon_match(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	if (avtyp != AV_400)
		return (0);

	return (1);
}

void
le_syscon_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct confargs *ca = aux;
	u_int etherpages;
	struct pglist pglist;
	vm_page_t pg;
	int rc;
	paddr_t pa;
	vaddr_t va;

	/*
	 * Allocate contiguous pages in the first 16MB to use as buffers.
	 */
	if (physmem >= atop(32 * 1024 * 1024))
		etherpages = 64;
	else if (physmem >= atop(16 * 1024 * 1024))
		etherpages = 32;
	else
		etherpages = 16;
	for (;;) {
		TAILQ_INIT(&pglist);
		rc = uvm_pglistalloc(ptoa(etherpages), 0, (1 << 24) - 1,
		    0, 0, &pglist, 1, UVM_PLA_NOWAIT);
		if (rc == 0)
			break;

		etherpages >>= 1;
		if (etherpages == 2) {
			printf(": no available memory, kernel is too large\n");
			return;
		}
	}

	va = uvm_km_valloc(kernel_map, ptoa(etherpages));
	if (va == NULL) {
		printf(": can't map descriptor memory\n");
		uvm_pglistfree(&pglist);
		return;
	}

	pa = VM_PAGE_TO_PHYS(TAILQ_FIRST(&pglist));

	sc->sc_mem = (void *)va;
	sc->sc_addr = (u_long)pa & 0x00ffffff;
	sc->sc_memsize = ptoa(etherpages);

	TAILQ_FOREACH(pg, &pglist, pageq) {
		pmap_enter(pmap_kernel(), va, pa,
		    UVM_PROT_RW, UVM_PROT_RW | PMAP_WIRED);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_cache_ctrl(pmap_kernel(), (vaddr_t)sc->sc_mem,
	    (vaddr_t)sc->sc_mem + sc->sc_memsize, CACHE_INH);
	pmap_update(pmap_kernel());

	lesc->sc_r1 = (struct av_lereg *)ca->ca_paddr;

	sc->sc_conf3 = LE_C3_BSWP;

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = le_syscon_rdcsr;
	sc->sc_wrcsr = le_syscon_wrcsr;
	sc->sc_hwreset = NULL;
	sc->sc_hwinit = NULL;

	am7990_config(sc);

	lesc->sc_ih.ih_fn = am7990_intr;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_flags = 0;
	lesc->sc_ih.ih_ipl = ca->ca_ipl;

	sysconintr_establish(INTSRC_ETHERNET1, &lesc->sc_ih, self->dv_xname);
}
