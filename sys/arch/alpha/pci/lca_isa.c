/*	$NetBSD: lca_isa.c,v 1.1 1995/11/23 02:37:40 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
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
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <dev/isa/isavar.h>

#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>

/*
 * Allocation/deallocation functions.
 */
int	lca_pio_alloc __P((void *, isa_iooffset_t, isa_iosize_t));
int	lca_pio_dealloc __P((void *, isa_iooffset_t, isa_iosize_t));

/*
 * Byte functions.
 */
isa_byte_t	lca_inb __P((void *, isa_iooffset_t));
#define	lca_insb	0					/* XXX */
void		lca_outb __P((void *, isa_iooffset_t, isa_byte_t));
#define	lca_outsb	0					/* XXX */

/*
 * Word functions.
 */
isa_word_t	lca_inw __P((void *, isa_iooffset_t));
#define	lca_insw	0					/* XXX */
void		lca_outw __P((void *, isa_iooffset_t, isa_word_t));
#define	lca_outsw	0					/* XXX */

/*
 * Longword functions.
 */
isa_long_t	lca_inl __P((void *, isa_iooffset_t));
#define	lca_insl	0					/* XXX */
void		lca_outl __P((void *, isa_iooffset_t, isa_long_t));
#define	lca_outsl	0					/* XXX */

__const struct pci_pio_fns lca_pio_fns = {
	/* Allocation/deallocation functions. */
	lca_pio_alloc,	lca_pio_dealloc,

	/* Byte functions. */
	lca_inb,	lca_insb,
	lca_outb,	lca_outsb,

	/* Word functions. */
	lca_inw,	lca_insw,
	lca_outw,	lca_outsw,

	/* Longword functions. */
	lca_inl,	lca_insl,
	lca_outl,	lca_outsl,
};

int
lca_pio_alloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

int
lca_pio_dealloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

isa_byte_t
lca_inb(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_byte_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 0 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;
/*	rval = val & 0xff; */

	return rval;
}

void
lca_outb(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_byte_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	nval = val << (8 * offset);
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 0 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_word_t
lca_inw(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_word_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 1 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;
	rval = val & 0xffff;

panic("inw(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

void
lca_outw(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_word_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 1 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_long_t
lca_inl(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_long_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 3 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffffffff;
	rval = val & 0xffffffff;

	return rval;
}

void
lca_outl(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_long_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(LCA_PCI_SIO | 3 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

/* XXX XXX XXX */

#define pf(fn, args)	fn args { panic(__STRING(fn)); }

void	pf(lca_dma_cascade, (void *idfa, isa_drq_t chan))
void	pf(lca_dma_copytobuf, ())
void	pf(lca_dma_copyfrombuf, ())
void	pf(lca_dma_start, (void *idfa, vm_offset_t addr,
	    isa_msize_t size, isa_drq_t chan, int flags))
void	pf(lca_dma_abort, (void *idfa, isa_drq_t chan))
void	pf(lca_dma_done, (void *idfa, isa_drq_t chan))

int	lca_dma_map __P((void *, vm_offset_t, isa_msize_t,
	    isa_moffset_t *, int));
void	lca_dma_unmap __P((void *, vm_offset_t, isa_msize_t, int,
	    isa_moffset_t *));

__const struct isa_dma_fns lca_dma_fns = {
	lca_dma_cascade,
	lca_dma_map,
	lca_dma_unmap,
	lca_dma_copytobuf,
	lca_dma_copyfrombuf,
	lca_dma_start,
	lca_dma_abort,
	lca_dma_done,
};

int
lca_dma_map(idfa, va, isasize, mappingsp, flags)
	void *idfa;
	vm_offset_t va;
	isa_msize_t isasize;
	isa_moffset_t *mappingsp;
	int flags;
{
	struct lca_config *acp = idfa;
	long todo;
	int i;

	if (ISA_DMA_NEEDCONTIG(flags) && isasize > NBPG ||
	    ISA_DMA_SIZEBOUND(flags) != ISA_DMA_SIZEBOUND_NONE ||
	    ISA_DMA_ADDRBOUND(flags) != ISA_DMA_ADDRBOUND_NONE)
		panic("lca_dma_map: punt");

	i = 0;
	todo = isasize;

	while (todo > 0) {
		mappingsp[i] = vtophys(va) | 0x40000000;
#if 0
		printf("a_pd_m mapping %d: %lx -> %lx -> %lx\n", i, va,
		    vtophys(va), mappingsp[i]);
#endif
		i++;
		todo -= PAGE_SIZE - (va - trunc_page(va));
		va += PAGE_SIZE - (va - trunc_page(va));
	}
	return (i);
}

void
lca_dma_unmap(idfa, va, isasize, nmappings, mappingsp)
	void *idfa;
	vm_offset_t va;
	isa_msize_t isasize;
	int nmappings;
	isa_moffset_t *mappingsp;
{

	printf("lca_dma_unmap: called\n");
}

vm_offset_t	lca_mem_map __P((void *, isa_moffset_t, isa_msize_t, int));
void		lca_mem_unmap __P((void *, vm_offset_t, isa_msize_t));

#if 0
void		lca_mem_copytoisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		lca_mem_copyfromisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		lca_mem_zero __P((void *, vm_offset_t, isa_moffset_t,
		    isa_msize_t));
#else
void		pf(lca_mem_copytoisa, ())
void		pf(lca_mem_copyfromisa, ())
void		pf(lca_mem_zero, ())
#endif

__const struct isa_mem_fns lca_mem_fns = {
	lca_mem_map,
	lca_mem_unmap,
	lca_mem_copytoisa,
	lca_mem_copyfromisa,
	lca_mem_zero,
};

vm_offset_t
lca_mem_map(imfa, isapa, isasize, cacheable)
	void *imfa;
	isa_moffset_t isapa;
	isa_msize_t isasize;
	int cacheable;
{
	vm_offset_t sbpa;

	/* XXX sanity checks on sizes, use of windows, etc. */

	/* XXX MAGIC NUMBERS */
	if (cacheable)
		sbpa = (isapa & 0xffffffff) | LCA_PCI_DENSE;
	else
		sbpa = ((isapa & 0x7ffffff) << 5) | LCA_PCI_SPARSE;

	return phystok0seg(sbpa);
}

void
lca_mem_unmap(imfa, va, isasize)
	void *imfa;
	vm_offset_t va;
	isa_msize_t isasize;
{

	/* XXX sanity checks on va */

	/* Nothing to do; mapping was done in direct-mapped segment. */
}
