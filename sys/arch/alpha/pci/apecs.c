/*	$NetBSD: apecs.c,v 1.3 1995/08/03 01:16:47 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <machine/rpb.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <alpha/isa/isa_dma.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/pci_chipset.h>
#include <alpha/pci/apecsreg.h>

int	apecsmatch __P((struct device *, void *, void *));
void	apecsattach __P((struct device *, struct device *, void *));

struct cfdriver apecscd = {
	NULL, "apecs", apecsmatch, apecsattach, DV_DULL, sizeof(struct device)
};

static int	apecsprint __P((void *, char *pnp));

#ifdef DEC_2100_A50
extern void	pci_2100_a50_pickintr __P((void));
#endif

#define	REGVAL(r)	(*(int32_t *)phystok0seg(r))

static int		nsgmapent = 1024;
static vm_offset_t	sgmap_pci_base = 0x800000;
/*static */struct sgmapent	*sgmap;
static char		/* * */ sgbitmap[1024 / NBBY];

int
apecsmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *pa = aux;

	/* XXX */

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
apecs_init()
{
	int pass2_epic;

	pass2_epic = (REGVAL(EPIC_DCSR) & EPIC_DCSR_PASS2) != 0;

	isadma_fcns = &apecs_isadma_fcns;
	isa_pio_fcns = &apecs_pio_fcns;
	if (!pass2_epic)
		pci_cs_fcns = &apecs_p1e_cs_fcns;
	else
		pci_cs_fcns = &apecs_p2e_cs_fcns;
}

void
apecsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct confargs nca;
	int pass2_comanche, widemem, pass2_epic;

	pass2_comanche = (REGVAL(COMANCHE_ED) & COMANCHE_ED_PASS2) != 0;
	widemem = (REGVAL(COMANCHE_GCR) & COMANCHE_GCR_WIDEMEM) != 0;
	pass2_epic = (REGVAL(EPIC_DCSR) & EPIC_DCSR_PASS2) != 0;

	sgmap = (struct sgmapent *)malloc(1024 * sizeof(struct sgmapent),
	    M_DEVBUF, M_WAITOK);

	printf(": DECchip %s Core Logic chipset\n",
	    widemem ? "21072" : "21071");
	printf("%s: DC21071-CA pass %d, %d-bit memory bus\n",
	    self->dv_xname, pass2_comanche ? 2 : 1, widemem ? 128 : 64);
	printf("%s: DC21071-DA pass %d\n", self->dv_xname, pass2_epic ? 2 : 1);
	/* XXX print bcache size */

	if (!pass2_epic)
		printf("WARNING: 21071-DA NOT PASS2... NO BETS...\n");

	/* set up the chipset's functions */
	apecs_init();

	switch (hwrpb->rpb_type) {
#if defined(DEC_2100_A50)
	case ST_DEC_2100_A50:
		pci_2100_a50_pickintr();
		break;
#endif
	default:
		panic("apecsattach: shouldn't be here, really...");
	}

	/* attach the PCI bus that hangs off of it... */
	nca.ca_name = "pci";
	nca.ca_slot = 0;
	nca.ca_offset = 0;
	nca.ca_bus = NULL;
	if (!config_found(self, &nca, apecsprint))
		panic("apecsattach: couldn't attach PCI bus");
}

static int
apecsprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        return (UNCONF);
}

vm_offset_t						/* XXX? */
apecs_sgmap_alloc(va, npg, nocross, waitok)
	caddr_t	va;
	int npg;
	vm_size_t nocross;
	int waitok;
{
	int s;
	int base, i, stride;

#ifdef DIAGNOSTIC
	/* Quick sanity checks. */
	if ((vm_offset_t)va & PGOFSET)
		panic("apecs_sgmap_alloc: va not page aligned");
	if ((nocross & (nocross - 1)) != 0 || nocross == 0)
		panic("apecs_sgmap_alloc: bogus alignment 0x%lx", nocross);
	if (npg <= 0)
		panic("apecs_sgmap_alloc: not allocating anything");
	if (npg > nsgmapent)
		panic("apecs_sgmap_alloc: insane allocation");
	if (ptoa(npg) > nocross)
		panic("apecs_sgmap_alloc: must cross boundary");
#endif

	stride = atop(nocross);
#ifdef DIAGNOSTIC
	if (stride > nsgmapent)
		panic("apecs_sgmap_alloc: cheesy implementation loses");
#endif

top:
	s = splhigh();
	for (base = 0; base < nsgmapent; base += stride) {
		for (i = base; i < base + npg; i++)
			if (isset(sgbitmap, i))
				goto nextstride;
		break;
nextstride:
	}
	if (base < nsgmapent)		/* found a free chunk, claim it */
		for (i = base; i < base + npg; i++)
			setbit(sgbitmap, i);
	splx(s);

	if (base >= nsgmapent) {	/* didn't find a free chunk */
		if (!waitok)
			return 0;
		tsleep(&sgmap, PRIBIO+1, "sgmap", 0);
		goto top;
	}

	for (i = base; i < base + npg; i++) {
#ifdef DIAGNOSTIC
		if ((sgmap[i].val & SGMAPENT_EVAL) != 0)
			panic("apecs_sgmap_alloc: unallocated entry valid");
#endif
		sgmap[i].val = SGMAP_MAKEENTRY(atop(vtophys(va)));
		va += PAGE_SIZE;
	}

	/* Invalidate old cached entries. */
	REGVAL(EPIC_TBIA) = 1;

	/* Return the PCI address. */
	return (ptoa(base) + sgmap_pci_base);
}

void
apecs_sgmap_dealloc(pa, npg)
	vm_offset_t pa;
	int npg;
{
	int i, pfn;

#ifdef DIAGNOSTIC
	/* Quick sanity checks. */
	if (pa & PGOFSET)
		panic("apecs_sgmap_dealloc: pa not page aligned");
	if (npg <= 0)
		panic("apecs_sgmap_dealloc: not deallocating anything");
	if (npg > nsgmapent)
		panic("apecs_sgmap_dealloc: insane deallocation");
#endif

	pfn = atop(pa - sgmap_pci_base);
#ifdef DIAGNOSTIC
	/* Bounds check the deallocation range.  Paranoid about wraparound. */
	if (pfn < 0 || pfn >= nsgmapent || (pfn + npg) >= nsgmapent)
		panic("apecs_sgmap_dealloc: pa out of range (%s)",
			pfn < 0 ? "too low" : "too high");
#endif

	for (i = 0; i < npg; i++) {
#ifdef DIAGNOSTIC
		/* Make sure it's actually allocated. */
		if (isclr(sgbitmap, i + pfn))
			panic("apecs_sgmap_dealloc: multiple frees: entry %d",
			    i + pfn);
#endif

		/* Clear the entries and the allocation map bits. */
		clrbit(sgbitmap, i + pfn);
		sgmap[i + pfn].val &= ~SGMAPENT_EVAL;
	}

	/* Invalidate old cached entries. */
	REGVAL(EPIC_TBIA) = 1;

	/* Wake up anybody waiting for map entries. */
	wakeup(&sgmap);
}
