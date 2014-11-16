/*	$OpenBSD: if_le_syscon.c,v 1.16 2014/11/16 12:30:56 deraadt Exp $	*/

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

#ifdef AV400
#include <machine/av400.h>
#endif
#ifdef AV530
#include <machine/av530.h>
#endif

#include <aviion/dev/sysconvar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>
/* #include <dev/ic/am79900reg.h> */
#include <dev/ic/am79900var.h>

/*
 * LANCE or ILACC registers. Although these are 16 bit registers, on the AV400
 * design, they need to be accessed as 32 bit registers. Bus magic...
 */
struct av_lereg {
	volatile uint32_t	ler1_rdp;	/* data port */
	volatile uint32_t	ler1_rap;	/* register select port */

	/* The following fields are only found on ILACC designs */
	volatile uint32_t	unused[2];
	volatile uint32_t	enaddr[6];
	volatile uint32_t	cksum[2];
};

struct le_softc {
	union {	/* glue to MI code */
		struct	lance_softc	sc_lance;
		struct	am7990_softc	sc_am7990;
		struct	am79900_softc	sc_am79900;
	} u;

	int	sc_ilacc;
	struct	av_lereg *sc_r1;	/* LANCE or ILACC registers */
	struct	intrhand sc_ih;
};

int	le_syscon_match(struct device *, void *, void *);
void	le_syscon_attach(struct device *, struct device *, void *);

struct cfattach le_syscon_ca = {
	sizeof(struct le_softc), le_syscon_match, le_syscon_attach
};

void	le_syscon_hwinit_ilacc(struct lance_softc *);
int	le_syscon_intr_ilacc(void *);
int	le_syscon_intr_lance(void *);
uint16_t le_syscon_rdcsr(struct lance_softc *, uint16_t);
void	le_syscon_wrcsr(struct lance_softc *, uint16_t, uint16_t);
void	le_syscon_wrcsr_interrupt(struct lance_softc *, uint16_t, uint16_t);

int
le_syscon_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
#ifdef AV530
	paddr_t fuse = 0;
#endif

	switch (cpuid) {
#ifdef AV400
	case AVIION_300_310:
	case AVIION_400_4000:
	case AVIION_410_4100:
	case AVIION_300C_310C:
	case AVIION_300CD_310CD:
	case AVIION_300D_310D:
	case AVIION_4300_25:
	case AVIION_4300_20:
	case AVIION_4300_16:
		switch (ca->ca_paddr) {
		case AV400_LAN:
			break;
		default:
			return 0;
		}
		break;
#endif
#ifdef AV530
	case AVIION_4600_530:
		switch (ca->ca_paddr) {
		case AV530_LAN1:
			fuse = AV530_IOFUSE0;
			break;
		case AV530_LAN2:
			fuse = AV530_IOFUSE1;
			break;
		default:
			return 0;
		}
		break;
#endif
	default:
		return 0;
	}

#ifdef AV530
	if (fuse != 0) {
		/* check IOFUSE register */
		if (badaddr(fuse, 1) != 0)
			return 0;

		/* check fuse status */
		return ISSET(*(volatile uint8_t *)fuse, AV530_IOFUSE_LAN);
	}
#endif

	return 1;
}

void
le_syscon_attach(struct device *parent, struct device *self, void *aux)
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct lance_softc *sc = &lesc->u.sc_lance;
	struct confargs *ca = aux;
	u_int etherpages;
	struct pglist pglist;
	vm_page_t pg;
	int i, rc;
	paddr_t pa, pamask;
	vaddr_t va;
	int intsrc;
	uint8_t *enaddr;

	switch (cpuid) {
#ifdef AV530
	case AVIION_4600_530:
		lesc->sc_ilacc = 1;
		pamask = 0xffffffff;
		intsrc = ca->ca_paddr == AV530_LAN1 ?
		    INTSRC_ETHERNET1 : INTSRC_ETHERNET2;
		break;
#endif
	default:
		lesc->sc_ilacc = 0;
		pamask = 0x00ffffff;
		intsrc = INTSRC_ETHERNET1;
		break;
	}

	/*
	 * Allocate contiguous pages (in the first 16MB if not ILACC)
	 * to use as buffers. We aim towards 256KB, which is as much as
	 * the VME LANCE boards provide.
	 */
	if (physmem >= atop(32 * 1024 * 1024))
		etherpages = atop(256 * 1024);
	else if (physmem >= atop(16 * 1024 * 1024))
		etherpages = atop(128 * 1024);
	else
		etherpages = atop(64 * 1024);
	for (;;) {
		TAILQ_INIT(&pglist);
		rc = uvm_pglistalloc(ptoa(etherpages), 0, pamask,
		    0, 0, &pglist, 1, UVM_PLA_NOWAIT);
		if (rc == 0)
			break;

		etherpages >>= 1;
		if (etherpages <= 2) {
			printf(": no available memory, kernel is too large\n");
			return;
		}
	}

	va = uvm_km_valloc(kernel_map, ptoa(etherpages));
	if (va == 0) {
		printf(": can't map descriptor memory\n");
		uvm_pglistfree(&pglist);
		return;
	}

	pa = VM_PAGE_TO_PHYS(TAILQ_FIRST(&pglist));

	sc->sc_mem = (void *)va;
	sc->sc_addr = (u_long)pa & pamask;
	sc->sc_memsize = ptoa(etherpages);

	TAILQ_FOREACH(pg, &pglist, pageq) {
		pmap_enter(pmap_kernel(), va, pa,
		    PROT_READ | PROT_WRITE,
		    PROT_READ | PROT_WRITE | PMAP_WIRED);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_cache_ctrl((vaddr_t)sc->sc_mem,
	    (vaddr_t)sc->sc_mem + sc->sc_memsize, CACHE_INH);
	pmap_update(pmap_kernel());

	lesc->sc_r1 = (struct av_lereg *)ca->ca_paddr;

	/*
	 * Get the device Ethernet address.
	 */
	enaddr = sc->sc_arpcom.ac_enaddr;
	if (lesc->sc_ilacc) {
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			enaddr[i] = lesc->sc_r1->enaddr[i] >> 24;
		/*
		 * If the checksum is invalid, don't trust the address,
		 * and force a hopefully unique one to be generated with
		 * ether_fakeaddr() at interface attachment time.
		 */
		if ((lesc->sc_r1->cksum[0] >> 24) !=
		    ((enaddr[0] + enaddr[2] + enaddr[4]) & 0xff) ||
		    (lesc->sc_r1->cksum[1] >> 24) !=
		    ((enaddr[1] + enaddr[3] + enaddr[5]) & 0xff))
			enaddr[0] = 0xff;
	} else
		myetheraddr(enaddr);

	sc->sc_conf3 = LE_C3_BSWP;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_syscon_rdcsr;
	sc->sc_wrcsr = le_syscon_wrcsr;
	sc->sc_hwreset = NULL;
	if (lesc->sc_ilacc)
		sc->sc_hwinit = le_syscon_hwinit_ilacc;
	else
		sc->sc_hwinit = NULL;

	if (lesc->sc_ilacc) {
		le_syscon_hwinit_ilacc(sc);
		am79900_config(&lesc->u.sc_am79900);
	} else
		am7990_config(&lesc->u.sc_am7990);

	if (lesc->sc_ilacc)
		lesc->sc_ih.ih_fn = le_syscon_intr_ilacc;
	else
		lesc->sc_ih.ih_fn = le_syscon_intr_lance;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_flags = 0;
	lesc->sc_ih.ih_ipl = IPL_NET;

	sysconintr_establish(intsrc, &lesc->sc_ih, self->dv_xname);
}

void
le_syscon_hwinit_ilacc(struct lance_softc *sc)
{
	le_syscon_wrcsr(sc, LE_CSR4,
	    LE_C4_DMAPLUS | LE_C4_UINT | LE_C4_TXSTRTM);
}

int
le_syscon_intr_ilacc(void *v)
{
	struct le_softc *lesc = (struct le_softc *)v;
	struct lance_softc *sc = &lesc->u.sc_lance;
	uint16_t csr4;

	/* Acknowledge TX start. XXX how to disable this? */
	csr4 = le_syscon_rdcsr(sc, LE_CSR4);
	if (csr4 & LE_C4_TXSTRT) {
		le_syscon_wrcsr(sc, LE_CSR4, csr4 | LE_C4_TXSTRTM);
		return 1;
	}

	return am79900_intr(v);
}

int
le_syscon_intr_lance(void *v)
{
	struct le_softc *lesc = (struct le_softc *)v;
	struct lance_softc *sc = &lesc->u.sc_lance;
	int rc;

	/*
	 * Syscon expects edge interrupts, while LANCE does level
	 * interrupts. To avoid missing interrupts while servicing,
	 * we disable further device interrupts while servicing.
	 *
	 * However, am7990_intr() will flip the interrupt enable bit
	 * itself; we override wrcsr with a specific version during
	 * servicing, so as not to reenable interrupts accidentally...
	 */
	sc->sc_wrcsr = le_syscon_wrcsr_interrupt;

	rc = am7990_intr(v);

	sc->sc_wrcsr = le_syscon_wrcsr;
	/*
	 * ...but we should not forget to reenable interrupts at this point!
	 */
	le_syscon_wrcsr(sc, LE_CSR0, LE_C0_INEA | le_syscon_rdcsr(sc, LE_CSR0));

	return rc;
}

uint16_t
le_syscon_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;
	uint16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return val;
}

void
le_syscon_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct av_lereg *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

void
le_syscon_wrcsr_interrupt(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	if (port == LE_CSR0)
		val &= ~LE_C0_INEA;

	le_syscon_wrcsr(sc, port, val);
}
