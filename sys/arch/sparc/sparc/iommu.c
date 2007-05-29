/*	$OpenBSD: iommu.c,v 1.20 2007/05/29 09:53:59 sobrado Exp $	*/
/*	$NetBSD: iommu.c,v 1.13 1997/07/29 09:42:04 fair Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1995 	Paul Kranenburg
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by Paul Kranenburg.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm.h>

#include <machine/pmap.h>

#include <machine/autoconf.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/iommureg.h>

struct iommu_softc {
	struct device	sc_dev;		/* base device */
	struct iommureg	*sc_reg;
	u_int		sc_pagesize;
	u_int		sc_range;
	u_int		sc_dvmabase;
	iopte_t		*sc_ptes;
	int		sc_hasiocache;
};
struct	iommu_softc *iommu_sc;/*XXX*/
int	has_iocache;


/* autoconfiguration driver */
int	iommu_print(void *, const char *);
void	iommu_attach(struct device *, struct device *, void *);
int	iommu_match(struct device *, void *, void *);

struct cfattach iommu_ca = {
	sizeof(struct iommu_softc), iommu_match, iommu_attach
};

struct cfdriver iommu_cd = {
	NULL, "iommu", DV_DULL
};

/*
 * Print the location of some iommu-attached device (called just
 * before attaching that device).  If `iommu' is not NULL, the
 * device was found but not configured; print the iommu as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
iommu_print(args, iommu)
	void *args;
	const char *iommu;
{
	register struct confargs *ca = args;

	if (iommu)
		printf("%s at %s", ca->ca_ra.ra_name, iommu);
	return (UNCONF);
}

int
iommu_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4OR4C)
		return (0);
	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach the iommu.
 */
void
iommu_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
#if defined(SUN4M)
	register struct iommu_softc *sc = (struct iommu_softc *)self;
	struct confargs oca, *ca = aux;
	register struct romaux *ra = &ca->ca_ra;
	register int node;
	register char *name;
	register u_int pbase, pa;
	register int i, mmupcrsave, s;
	register iopte_t *tpte_p;
	struct pglist mlist;
	struct vm_page *m;
	vaddr_t va;
	paddr_t iopte_pa;

	iommu_sc = sc;
	/*
	 * XXX there is only one iommu, for now -- do not know how to
	 * address children on others
	 */
	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	node = ra->ra_node;

#if 0
	if (ra->ra_vaddr)
		sc->sc_reg = (struct iommureg *)ca->ca_ra.ra_vaddr;
#else
	/*
	 * Map registers into our space. The PROM may have done this
	 * already, but I feel better if we have our own copy. Plus, the
	 * prom doesn't map the entire register set
	 *
	 * XXX struct iommureg is bigger than ra->ra_len; what are the
	 *     other fields for?
	 */
	sc->sc_reg = (struct iommureg *)
		mapiodev(ra->ra_reg, 0, ra->ra_len);
#endif

	sc->sc_hasiocache = node_has_property(node, "cache-coherence?");
	if (CACHEINFO.c_enabled == 0) /* XXX - is this correct? */
		sc->sc_hasiocache = 0;
	has_iocache = sc->sc_hasiocache; /* Set global flag */

	sc->sc_pagesize = getpropint(node, "page-size", NBPG),
	sc->sc_range = (1 << 24) <<
	    ((sc->sc_reg->io_cr & IOMMU_CTL_RANGE) >> IOMMU_CTL_RANGESHFT);
#if 0
	sc->sc_dvmabase = (0 - sc->sc_range);
#endif
	pbase = (sc->sc_reg->io_bar & IOMMU_BAR_IBA) <<
			(14 - IOMMU_BAR_IBASHFT);

	/*
	 * Allocate memory for I/O pagetables. This takes 64k of memory
	 * since we want to have 64M of dvma space (this actually depends
	 * on the definition of DVMA4M_BASE...we may drop it back to 32M).
	 * The table must be aligned on a (-DVMA4M_BASE/NBPG) boundary
	 * (i.e. 64K for 64M of dvma space).
	 */
	TAILQ_INIT(&mlist);
#define DVMA_PTESIZE ((0 - DVMA4M_BASE) / 1024)
	if (uvm_pglistalloc(DVMA_PTESIZE, 0, 0xffffffff, DVMA_PTESIZE,
			    0, &mlist, 1, 0) ||
	    (va = uvm_km_valloc(kernel_map, DVMA_PTESIZE)) == 0)
		panic("iommu_attach: can't allocate memory for pagetables");
#undef DVMA_PTESIZE
	m = TAILQ_FIRST(&mlist);
	iopte_pa = VM_PAGE_TO_PHYS(m);
	sc->sc_ptes = (iopte_t *) va;

	while (m) {
		paddr_t pa = VM_PAGE_TO_PHYS(m);
		pmap_kenter_pa(va, pa | PMAP_NC, VM_PROT_READ|VM_PROT_WRITE);
		va += PAGE_SIZE;
		m = TAILQ_NEXT(m, pageq);
	}
	pmap_update(pmap_kernel());

	/*
	 * Now we build our own copy of the IOMMU page tables. We need to
	 * do this since we're going to change the range to give us 64M of
	 * mappings, and thus we can move DVMA space down to 0xfd000000 to
	 * give us lots of space and to avoid bumping into the PROM, etc.
	 *
	 * XXX Note that this is rather messy.
	 */

	/*
	 * Ok. We've got to read in the original table using MMU bypass,
	 * and copy all of its entries to the appropriate place in our
	 * new table, even if the sizes are different.
	 * This is pretty easy since we know DVMA ends at 0xffffffff.
	 *
	 * XXX: PGOFSET, NBPG assume same page size as SRMMU
	 */
	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		/* set MMU AC bit */
		sta(SRMMU_PCR, ASI_SRMMU,
		    ((mmupcrsave = lda(SRMMU_PCR, ASI_SRMMU)) | VIKING_PCR_AC));
	}

	for (tpte_p = &sc->sc_ptes[((0 - DVMA4M_BASE)/NBPG) - 1],
	     pa = (u_int)pbase - sizeof(iopte_t) +
		   ((u_int)sc->sc_range/NBPG)*sizeof(iopte_t);
	     tpte_p >= &sc->sc_ptes[0] && pa >= (u_int)pbase;
	     tpte_p--, pa -= sizeof(iopte_t)) {

		IOMMU_FLUSHPAGE(sc,
			        (tpte_p - &sc->sc_ptes[0])*NBPG + DVMA4M_BASE);
		*tpte_p = lda(pa, ASI_BYPASS);
	}
	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		/* restore mmu after bug-avoidance */
		sta(SRMMU_PCR, ASI_SRMMU, mmupcrsave);
	}

	/*
	 * Now we can install our new pagetable into the IOMMU
	 */
	sc->sc_range = 0 - DVMA4M_BASE;
	sc->sc_dvmabase = DVMA4M_BASE;

	/* calculate log2(sc->sc_range/16MB) */
	i = ffs(sc->sc_range/(1 << 24)) - 1;
	if ((1 << i) != (sc->sc_range/(1 << 24)))
		panic("bad iommu range: %d",i);

	s = splhigh();
	IOMMU_FLUSHALL(sc);

	sc->sc_reg->io_cr = (sc->sc_reg->io_cr & ~IOMMU_CTL_RANGE) |
			  (i << IOMMU_CTL_RANGESHFT) | IOMMU_CTL_ME;
	sc->sc_reg->io_bar = (iopte_pa >> 4) & IOMMU_BAR_IBA;

	IOMMU_FLUSHALL(sc);
	splx(s);

	printf(": version 0x%x/0x%x, page-size %d, range %dMB\n",
		(sc->sc_reg->io_cr & IOMMU_CTL_VER) >> 24,
		(sc->sc_reg->io_cr & IOMMU_CTL_IMPL) >> 28,
		sc->sc_pagesize,
		sc->sc_range >> 20);

	/* Propagate bootpath */
	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "iommu") == 0)
		oca.ca_ra.ra_bp = ra->ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	/*
	 * Loop through ROM children (expect SBus among them).
	 */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;
		oca.ca_bustype = BUS_MAIN; /* ??? */
		(void) config_found(&sc->sc_dev, (void *)&oca, iommu_print);
	}
#endif
}

void
iommu_enter(va, pa)
	u_int va, pa;
{
	struct iommu_softc *sc = iommu_sc;
	int pte;

#ifdef DEBUG
	if (va < sc->sc_dvmabase)
		panic("iommu_enter: va 0x%x not in DVMA space",va);
#endif

	pte = atop(pa) << IOPTE_PPNSHFT;
	pte &= IOPTE_PPN;
	pte |= IOPTE_V | IOPTE_W | (has_iocache ? IOPTE_C : 0);
	sc->sc_ptes[atop(va - sc->sc_dvmabase)] = pte;
	IOMMU_FLUSHPAGE(sc, va);
}

/*
 * iommu_remove: clears mappings created by iommu_enter
 */
void
iommu_remove(va, len)
	register u_int va, len;
{
	register struct iommu_softc *sc = iommu_sc;

#ifdef DEBUG
	if (va < sc->sc_dvmabase)
		panic("iommu_enter: va 0x%x not in DVMA space", va);
#endif

	while (len > 0) {
#ifdef notyet
#ifdef DEBUG
		if ((sc->sc_ptes[atop(va - sc->sc_dvmabase)] & IOPTE_V) == 0)
			panic("iommu_remove: clearing invalid pte at va 0x%x",
				va);
#endif
#endif
		sc->sc_ptes[atop(va - sc->sc_dvmabase)] = 0;
		IOMMU_FLUSHPAGE(sc, va);
		len -= sc->sc_pagesize;
		va += sc->sc_pagesize;
	}
}
