/*	$OpenBSD: vsbus.c,v 1.4 2000/04/27 00:52:07 bjc Exp $ */
/*	$NetBSD: vsbus.c,v 1.20 1999/10/22 21:10:12 ragge Exp $ */
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
#include <sys/map.h>
#include <sys/device.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/stat.h>

#include <vm/vm.h>

#define	_VAX_BUS_DMA_PRIVATE
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
#include <machine/ka43.h>

#include <machine/vsbus.h>

int		vsbus_match		__P((struct device *, struct cfdata *, void *));
void	vsbus_attach	__P((struct device *, struct device *, void *));
int		vsbus_print		__P((void *, const char *));
int		vsbus_search	__P((struct device *, void *, void *));

void	ka410_attach	__P((struct device *, struct device *, void *));
void	ka43_attach	__P((struct device *, struct device *, void *));

struct vax_bus_dma_tag vsbus_bus_dma_tag = {
	0,
	0,
	0,
	0,
	0,
	0,
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

struct	cfattach vsbus_ca = { 
	sizeof(struct vsbus_softc), (cfmatch_t)vsbus_match, vsbus_attach
};

struct  cfdriver vsbus_cd = {
	    NULL, "vsbus", DV_DULL
};

/* dummy interrupt handler for use during autoconf */
void
vsbus_intr(arg)
	void *arg;
{
	return;
}

int
vsbus_print(aux, name)
	void *aux;
	const char *name;
{
	struct vsbus_attach_args *va = aux;

	printf(" csr 0x%lx vec 0x%x ipl %x maskbit %d", va->va_paddr,
	    va->va_cvec & 511, va->va_br, va->va_maskno - 1);
	return(UNCONF); 
}

int
vsbus_match(parent, cf, aux)
	struct	device	*parent;
	struct 	cfdata	*cf;
	void	*aux;
{
	if (vax_bustype == VAX_VSBUS)
		return 1;
	return 0;
}

void
vsbus_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct	vsbus_softc *sc = (void *)self;
	int		discard;
	vaddr_t temp;

	printf("\n");

	switch (vax_boardtype) {
	case VAX_BTYP_49:
		temp = vax_map_physmem(0x25c00000, 1);
		sc->sc_intreq = (char *)temp + 12;
		sc->sc_intclr = (char *)temp + 12;
		sc->sc_intmsk = (char *)temp + 8;
		break;

	default:
		temp = vax_map_physmem(VS_REGS, 1);
		sc->sc_intreq = (char *)temp + 15;
		sc->sc_intclr = (char *)temp + 15;
		sc->sc_intmsk = (char *)temp + 12;
		break;
	}

	/*
	 * First: find which interrupts we won't care about.
	 * There are interrupts that interrupt on a periodic basic
	 * that we don't want to interfere with the rest of the 
	 * interrupt probing.
	 */
	*sc->sc_intmsk = 0;
	*sc->sc_intclr = 0xff;
	DELAY(1000000); /* Wait a second */
	sc->sc_mask = discard = *sc->sc_intreq;
	printf("%s: interrupt mask %x\n", self->dv_xname, discard);

	/*
	 * now check for all possible devices on this "bus"
	 */
	config_search(vsbus_search, self, NULL);

	*sc->sc_intmsk = sc->sc_mask ^ discard;
}

int
vsbus_search(parent, cfd, aux)
	struct device	*parent;
	void 	*cfd;
	void 	*aux;
{
	struct	vsbus_softc 		*sc = (void *)parent;
	struct	vsbus_attach_args 	va;
	struct 	cfdata	*cf = cfd;
	int 	i, vec, br;
	u_char	c;

	va.va_paddr = cf->cf_loc[0];
	va.va_addr = vax_map_physmem(va.va_paddr, 1);
	va.va_dmat = &vsbus_bus_dma_tag;

	*sc->sc_intmsk = 0;
	*sc->sc_intclr = 0xff;
	scb_vecref(0, 0); /* Clear vector ref */

	va.va_ivec = vsbus_intr;
	i = (*cf->cf_attach->ca_match) (parent, cf, &va);
	vax_unmap_physmem(va.va_addr, 1);
	c = *sc->sc_intreq & ~sc->sc_mask;
	if (i == 0)
		goto forgetit;
	if (i > 10)
		c = sc->sc_mask; /* Fooling interrupt */
	else if (c == 0)
		goto forgetit;

	va.va_maskno = ffs((u_int)c);

	*sc->sc_intmsk = c;
	DELAY(1000);
	*sc->sc_intmsk = 0;

	i = scb_vecref(&vec, &br);
	if (i == 0)
		goto fail;
	if (vec == 0)
		goto fail;
	
	scb_vecalloc(vec, va.va_ivec, va.va_vecarg, SCB_ISTACK);
	va.va_br = br;
	va.va_cvec = vec;
	va.confargs = aux;		

	config_attach(parent, cf, &va, vsbus_print);
	return 1;

fail:
	printf("%s%d at %s csr %x %s\n",
	    cf->cf_driver->cd_name, cf->cf_unit, parent->dv_xname,
	    cf->cf_loc[0], (i ? "zero vector" : "didn't interrupt"));
forgetit:
	return 0;
}

static volatile struct dma_lock {
    int dl_locked;
    int dl_wanted;
    void    *dl_owner;
    int dl_count;
} dmalock = { 0, 0, NULL, 0 };

int
vsbus_lockDMA(ca)
    struct confargs *ca;
{
    while (dmalock.dl_locked) {
        dmalock.dl_wanted++;
        sleep((caddr_t)&dmalock, PRIBIO);   /* PLOCK or PRIBIO ? */
        dmalock.dl_wanted--;
    }
    dmalock.dl_locked++;
    dmalock.dl_owner = ca;

    /*
     * no checks yet, no timeouts, nothing...
     */

#ifdef DEBUG
    if ((++dmalock.dl_count % 1000) == 0)
        printf("%d locks, owner: %s\n", dmalock.dl_count, ca->ca_name);
#endif
    return (0);
}

int
vsbus_unlockDMA(ca)
    struct confargs *ca;
{
    if (dmalock.dl_locked != 1 || dmalock.dl_owner != ca) {
        printf("locking-problem: %d, %s\n", dmalock.dl_locked,
               (dmalock.dl_owner ? dmalock.dl_owner : "null"));
        dmalock.dl_locked = 0;
        return (-1);
    }
    dmalock.dl_owner = NULL;
    dmalock.dl_locked = 0;
    if (dmalock.dl_wanted) {
        wakeup((caddr_t)&dmalock);
    }
    return (0);
}


/*
 * Sets a new interrupt mask. Returns the old one.
 * Works like spl functions.
 */
unsigned char
vsbus_setmask(mask)
	unsigned char mask;
{
	struct vsbus_softc *sc = vsbus_cd.cd_devs[0];
	unsigned char ch;

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
	struct vsbus_softc *sc = vsbus_cd.cd_devs[0];

	*sc->sc_intclr = mask;
}

/*
 * Copy data from/to a user process' space from the DMA area.
 * Use the physical memory directly.
 */
void
vsbus_copytoproc(p, from, to, len)
	struct proc *p;
	caddr_t from, to;
	int len;
{
	struct pte *pte;
	paddr_t pa;

	pte = uvtopte(TRUNC_PAGE(to), (&p->p_addr->u_pcb));
	if ((vaddr_t)to & PGOFSET) {
		int cz = ROUND_PAGE(to) - (vaddr_t)to;

		pa = (pte->pg_pfn << VAX_PGSHIFT) | (NBPG - cz) | KERNBASE;
		bcopy(from, (caddr_t)pa, min(cz, len));
		from += cz;
		to += cz;
		len -= cz;
		pte += 8; /* XXX */
	}
	while (len > 0) {
		pa = (pte->pg_pfn << VAX_PGSHIFT) | KERNBASE;
		bcopy(from, (caddr_t)pa, min(NBPG, len));
		from += NBPG;
		to += NBPG;
		len -= NBPG;
		pte += 8; /* XXX */
	}
}

void
vsbus_copyfromproc(p, from, to, len)
	struct proc *p;
	caddr_t from, to;
	int len;
{
	struct pte *pte;
	paddr_t pa;

	pte = uvtopte(TRUNC_PAGE(from), (&p->p_addr->u_pcb));
	if ((vaddr_t)from & PGOFSET) {
		int cz = ROUND_PAGE(from) - (vaddr_t)from;

		pa = (pte->pg_pfn << VAX_PGSHIFT) | (NBPG - cz) | KERNBASE;
		bcopy((caddr_t)pa, to, min(cz, len));
		from += cz;
		to += cz;
		len -= cz;
		pte += 8; /* XXX */
	}
	while (len > 0) {
		pa = (pte->pg_pfn << VAX_PGSHIFT) | KERNBASE;
		bcopy((caddr_t)pa, to, min(NBPG, len));
		from += NBPG;
		to += NBPG;
		len -= NBPG;
		pte += 8; /* XXX */
	}
}
