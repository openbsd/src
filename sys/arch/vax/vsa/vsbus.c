/*	$OpenBSD: vsbus.c,v 1.21 2010/11/18 21:13:19 miod Exp $ */
/*	$NetBSD: vsbus.c,v 1.29 2000/06/29 07:14:37 mrg Exp $ */
/*
 * Copyright (c) 1996, 1999 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/stat.h>

#include <uvm/uvm_extern.h>

#define _VAX_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/nexus.h>

#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/ka420.h>

#include <machine/vsbus.h>

int	vsbus_match(struct device *, struct cfdata *, void *);
void	vsbus_attach(struct device *, struct device *, void *);
int	vsbus_print(void *, const char *);
int	vsbus_search(struct device *, void *, void *);

static struct vax_bus_dma_tag vsbus_bus_dma_tag = {
	NULL,
	0,
	0,
	0,
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

extern struct vax_bus_space vax_mem_bus_space;
static SIMPLEQ_HEAD(, vsbus_dma) vsbus_dma;

struct	cfattach vsbus_ca = { 
	sizeof(struct vsbus_softc), (cfmatch_t)vsbus_match, vsbus_attach
};

struct  cfdriver vsbus_cd = {
	    NULL, "vsbus", DV_DULL
};

int	oldvsbus;

int
vsbus_print(aux, name)
	void *aux;
	const char *name;
{
	struct vsbus_attach_args *va = aux;

	printf(" csr 0x%lx vec %d ipl %x maskbit %d", va->va_paddr,
	    va->va_cvec & 511, va->va_br, va->va_maskno - 1);
	return(UNCONF); 
}

int
vsbus_match(parent, cf, aux)
	struct	device	*parent;
	struct	cfdata	*cf;
	void	*aux;
{
	struct mainbus_attach_args *maa = aux;

	if (maa->maa_bustype == VAX_VSBUS)
		return 1;
	return 0;
}

void
vsbus_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct	vsbus_softc *sc = (void *)self;
	int dbase, dsize;

	printf("\n");

	sc->sc_dmatag = vsbus_bus_dma_tag;

	switch (vax_boardtype) {
#if VAX49 || VAX53
	case VAX_BTYP_1303:
	case VAX_BTYP_49:
		sc->sc_vsregs = vax_map_physmem(0x25c00000, 1);
		sc->sc_intreq = (char *)sc->sc_vsregs + 12;
		sc->sc_intclr = (char *)sc->sc_vsregs + 12;
		sc->sc_intmsk = (char *)sc->sc_vsregs + 8;
		vsbus_dma_init(sc, 8192);
		break;
#endif

#if VAX46 || VAX48
	case VAX_BTYP_48:
	case VAX_BTYP_46:
		sc->sc_vsregs = vax_map_physmem(VS_REGS, 1);
		sc->sc_intreq = (char *)sc->sc_vsregs + 15;
		sc->sc_intclr = (char *)sc->sc_vsregs + 15;
		sc->sc_intmsk = (char *)sc->sc_vsregs + 12;
		vsbus_dma_init(sc, 32768);
		break;
#endif

	default:
		sc->sc_vsregs = vax_map_physmem(VS_REGS, 1);
		sc->sc_intreq = (char *)sc->sc_vsregs + 15;
		sc->sc_intclr = (char *)sc->sc_vsregs + 15;
		sc->sc_intmsk = (char *)sc->sc_vsregs + 12;
		if (vax_boardtype == VAX_BTYP_410) {
			dbase = KA410_DMA_BASE;
			dsize = KA410_DMA_SIZE;
		} else {
			dbase = KA420_DMA_BASE;
			dsize = KA420_DMA_SIZE;
			*(char *)(sc->sc_vsregs + 0xe0) = 1; /* Big DMA */
		}
		sc->sc_dmasize = dsize;
		sc->sc_dmaaddr = uvm_km_valloc(kernel_map, dsize);
		ioaccess(sc->sc_dmaaddr, dbase, dsize/VAX_NBPG);
		break;
	}

	SIMPLEQ_INIT(&vsbus_dma);
	/*
	 * First: find which interrupts we won't care about.
	 * There are interrupts that interrupt on a periodic basic
	 * that we don't want to interfere with the rest of the 
	 * interrupt probing.
	 */
	*sc->sc_intmsk = 0;
	*sc->sc_intclr = 0xff;
	DELAY(1000000); /* Wait a second */
	sc->sc_mask = *sc->sc_intreq;

#if VAX48
	/*
	 * It's possible for the 4000/VLC to generate an DZ-11 rx interrupt
	 * (0x20) during the delay period, unmask that bit.
	 */
	if (vax_boardtype == VAX_BTYP_48)
		sc->sc_mask &= ~0x20;
#endif

	printf("%s: interrupt mask %x\n", self->dv_xname, sc->sc_mask);

	/*
	 * now check for all possible devices on this "bus"
	 */
	config_search(vsbus_search, self, NULL);

	/* Autoconfig finished, enable interrupts */
	*sc->sc_intmsk = ~sc->sc_mask;
}

int
vsbus_search(parent, cfd, aux)
	struct device *parent;
	void *cfd;
	void *aux;
{
	struct	vsbus_softc *sc = (void *)parent;
	struct	vsbus_attach_args va;
	struct	cfdata *cf = cfd;
	int i, vec, br;
	u_char c;

	va.va_paddr = cf->cf_loc[0];
	va.va_addr = vax_map_physmem(va.va_paddr, 1);
	va.va_dmat = &sc->sc_dmatag;
	va.va_iot = &vax_mem_bus_space;

	*sc->sc_intmsk = 0;
	*sc->sc_intclr = 0xff;
	scb_vecref(0, 0); /* Clear vector ref */

	i = (*cf->cf_attach->ca_match) (parent, cf, &va);
	vax_unmap_physmem(va.va_addr, 1);
	c = *sc->sc_intreq & ~sc->sc_mask;

	if (i == 0)
		goto forgetit;
	if (i > 10)
		c = sc->sc_mask; /* Fooling interrupt */
	else if (c == 0)
		goto forgetit;

	*sc->sc_intmsk = c;
	DELAY(1000);
	*sc->sc_intmsk = 0;
	va.va_maskno = ffs((u_int)c);
	i = scb_vecref(&vec, &br);
	if (i == 0)
		goto fail;
	if (vec == 0)
		goto fail;

	/*
	 * For proper splassert operation, we need to know if we are on
	 * a vsbus system where its devices interrupt at level 0x14 instead
	 * of 0x15.
	 */
	if (br == 0x14)
		oldvsbus = 1;

	va.va_br = br;
	va.va_cvec = vec;
	va.va_dmaaddr = sc->sc_dmaaddr;
	va.va_dmasize = sc->sc_dmasize;
	*sc->sc_intmsk = c; /* Allow interrupts during attach */
	config_attach(parent, cf, &va, vsbus_print);
	*sc->sc_intmsk = 0;
	return 0;

fail:
	printf("%s%d at %s csr 0x%x %s\n",
	    cf->cf_driver->cd_name, cf->cf_unit, parent->dv_xname,
	    cf->cf_loc[0], (i ? "zero vector" : "didn't interrupt"));
forgetit:
	return 0;
}

/*
 * Sets a new interrupt mask. Returns the old one.
 * Works like spl functions.
 */
unsigned char
vsbus_setmask(mask)
	unsigned char mask;
{
	struct vsbus_softc *sc;
	unsigned char ch;

	if (vsbus_cd.cd_ndevs == 0)
		return 0;
	sc = vsbus_cd.cd_devs[0];

	ch = *sc->sc_intmsk;
	*sc->sc_intmsk = mask;
	return ch;
}

/*
 * Clears the interrupts in mask.
 */
void
vsbus_clrintr(mask)
	unsigned char mask;
{
	struct vsbus_softc *sc;

	if (vsbus_cd.cd_ndevs == 0)
		return;
	sc = vsbus_cd.cd_devs[0];

	*sc->sc_intclr = mask;
}

/*
 * Copy data from/to a user process' space from the DMA area.
 * Use the physical memory directly.
 */
void
vsbus_copytoproc(struct proc *p, caddr_t from, caddr_t to, int len)
{
	pt_entry_t *pte;
	paddr_t pa;

	if ((vaddr_t)to & KERNBASE) { /* In kernel space */
		bcopy(from, to, len);
		return;
	}
	pte = uvtopte(trunc_page((vaddr_t)to), (&p->p_addr->u_pcb));
	if ((vaddr_t)to & PGOFSET) {
		int cz = round_page((vaddr_t)to) - (vaddr_t)to;

		pa = ((*pte & PG_FRAME) << VAX_PGSHIFT) |
		    (NBPG - cz) | KERNBASE;
		bcopy(from, (caddr_t)pa, min(cz, len));
		from += cz;
		to += cz;
		len -= cz;
		pte += 8; /* XXX */
	}
	while (len > 0) {
		pa = ((*pte & PG_FRAME) << VAX_PGSHIFT) | KERNBASE;
		bcopy(from, (caddr_t)pa, min(NBPG, len));
		from += NBPG;
		to += NBPG;
		len -= NBPG;
		pte += 8; /* XXX */
	}
}

void
vsbus_copyfromproc(struct proc *p, caddr_t from, caddr_t to, int len)
{
	pt_entry_t *pte;
	paddr_t pa;

	if ((vaddr_t)from & KERNBASE) { /* In kernel space */
		bcopy(from, to, len);
		return;
	}
	pte = uvtopte(trunc_page((vaddr_t)from), (&p->p_addr->u_pcb));
	if ((vaddr_t)from & PGOFSET) {
		int cz = round_page((vaddr_t)from) - (vaddr_t)from;

		pa = ((*pte & PG_FRAME) << VAX_PGSHIFT) |
		    (NBPG - cz) | KERNBASE;
		bcopy((caddr_t)pa, to, min(cz, len));
		from += cz;
		to += cz;
		len -= cz;
		pte += 8; /* XXX */
	}
	while (len > 0) {
		pa = ((*pte & PG_FRAME) << VAX_PGSHIFT) | KERNBASE;
		bcopy((caddr_t)pa, to, min(NBPG, len));
		from += NBPG;
		to += NBPG;
		len -= NBPG;
		pte += 8; /* XXX */
	}
}

/* 
 * There can only be one user of the DMA area on VS2k/VS3100 at one
 * time, so keep track of it here.
 */ 
static int vsbus_active = 0;

void
vsbus_dma_start(struct vsbus_dma *vd)
{
 
	SIMPLEQ_INSERT_TAIL(&vsbus_dma, vd, vd_q);

	if (vsbus_active == 0)
		vsbus_dma_intr();
}
 
void
vsbus_dma_intr(void)
{	
	struct vsbus_dma *vd;
	
	vd = SIMPLEQ_FIRST(&vsbus_dma); 
	if (vd == NULL) {
		vsbus_active = 0;
		return;
	}
	vsbus_active = 1;
	SIMPLEQ_REMOVE_HEAD(&vsbus_dma, vd_q);
	(*vd->vd_go)(vd->vd_arg);
}

