/*	$NetBSD: apecs_isa.c,v 1.4 1995/11/23 02:37:13 cgd Exp $	*/

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
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <dev/isa/isavar.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

/*
 * Allocation/deallocation functions.
 */
int	apecs_pio_alloc __P((void *, isa_iooffset_t, isa_iosize_t));
int	apecs_pio_dealloc __P((void *, isa_iooffset_t, isa_iosize_t));

/*
 * Byte functions.
 */
isa_byte_t	apecs_inb __P((void *, isa_iooffset_t));
#define	apecs_insb	0					/* XXX */
void		apecs_outb __P((void *, isa_iooffset_t, isa_byte_t));
#define	apecs_outsb	0					/* XXX */

/*
 * Word functions.
 */
isa_word_t	apecs_inw __P((void *, isa_iooffset_t));
#define	apecs_insw	0					/* XXX */
void		apecs_outw __P((void *, isa_iooffset_t, isa_word_t));
#define	apecs_outsw	0					/* XXX */

/*
 * Longword functions.
 */
isa_long_t	apecs_inl __P((void *, isa_iooffset_t));
#define	apecs_insl	0					/* XXX */
void		apecs_outl __P((void *, isa_iooffset_t, isa_long_t));
#define	apecs_outsl	0					/* XXX */

__const struct pci_pio_fns apecs_pio_fns = {
	/* Allocation/deallocation functions. */
	apecs_pio_alloc,	apecs_pio_dealloc,

	/* Byte functions. */
	apecs_inb,	apecs_insb,
	apecs_outb,	apecs_outsb,

	/* Word functions. */
	apecs_inw,	apecs_insw,
	apecs_outw,	apecs_outsw,

	/* Longword functions. */
	apecs_inl,	apecs_insl,
	apecs_outl,	apecs_outsl,
};

int
apecs_pio_alloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

int
apecs_pio_dealloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

isa_byte_t
apecs_inb(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_byte_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 0 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;
/*	rval = val & 0xff; */

	return rval;
}

void
apecs_outb(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_byte_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	nval = val << (8 * offset);
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 0 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_word_t
apecs_inw(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_word_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 1 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;
	rval = val & 0xffff;

panic("inw(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

void
apecs_outw(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_word_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 1 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_long_t
apecs_inl(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_long_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 3 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffffffff;
	rval = val & 0xffffffff;

	return rval;
}

void
apecs_outl(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_long_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 3 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

/* XXX XXX XXX */

#define pf(fn, args)	fn args { panic(__STRING(fn)); }

void	pf(apecs_dma_cascade, (void *idfa, isa_drq_t chan))
void	pf(apecs_dma_copytobuf, ())
void	pf(apecs_dma_copyfrombuf, ())
void	pf(apecs_dma_start, (void *idfa, vm_offset_t addr,
	    isa_msize_t size, isa_drq_t chan, int flags))
void	pf(apecs_dma_abort, (void *idfa, isa_drq_t chan))
void	pf(apecs_dma_done, (void *idfa, isa_drq_t chan))

int	apecs_dma_map __P((void *, vm_offset_t, isa_msize_t,
	    isa_moffset_t *, int));
void	apecs_dma_unmap __P((void *, vm_offset_t, isa_msize_t, int,
	    isa_moffset_t *));

__const struct isa_dma_fns apecs_dma_fns = {
	apecs_dma_cascade,
	apecs_dma_map,
	apecs_dma_unmap,
	apecs_dma_copytobuf,
	apecs_dma_copyfrombuf,
	apecs_dma_start,
	apecs_dma_abort,
	apecs_dma_done,
};

int
apecs_dma_map(idfa, va, isasize, mappingsp, flags)
	void *idfa;
	vm_offset_t va;
	isa_msize_t isasize;
	isa_moffset_t *mappingsp;
	int flags;
{
	struct apecs_config *acp = idfa;
	long todo;
	int i;

	if (ISA_DMA_NEEDCONTIG(flags) && isasize > NBPG ||
	    ISA_DMA_SIZEBOUND(flags) != ISA_DMA_SIZEBOUND_NONE ||
	    ISA_DMA_ADDRBOUND(flags) != ISA_DMA_ADDRBOUND_NONE)
		panic("apecs_dma_map: punt");

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
apecs_dma_unmap(idfa, va, isasize, nmappings, mappingsp)
	void *idfa;
	vm_offset_t va;
	isa_msize_t isasize;
	int nmappings;
	isa_moffset_t *mappingsp;
{

	printf("apecs_dma_unmap: called\n");
}

vm_offset_t	apecs_mem_map __P((void *, isa_moffset_t, isa_msize_t, int));
void		apecs_mem_unmap __P((void *, vm_offset_t, isa_msize_t));

#if 0
void		apecs_mem_copytoisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		apecs_mem_copyfromisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		apecs_mem_zero __P((void *, vm_offset_t, isa_moffset_t,
		    isa_msize_t));
#else
void		pf(apecs_mem_copytoisa, ())
void		pf(apecs_mem_copyfromisa, ())
void		pf(apecs_mem_zero, ())
#endif

__const struct isa_mem_fns apecs_mem_fns = {
	apecs_mem_map,
	apecs_mem_unmap,
	apecs_mem_copytoisa,
	apecs_mem_copyfromisa,
	apecs_mem_zero,
};

vm_offset_t
apecs_mem_map(imfa, isapa, isasize, cacheable)
	void *imfa;
	isa_moffset_t isapa;
	isa_msize_t isasize;
	int cacheable;
{
	vm_offset_t sbpa;

	/* XXX sanity checks on sizes, use of windows, etc. */

	/* XXX MAGIC NUMBERS */
	if (cacheable)
		sbpa = (isapa & 0xffffffff) | APECS_PCI_DENSE;
	else
		sbpa = ((isapa & 0x7ffffff) << 5) | APECS_PCI_SPARSE;

	return phystok0seg(sbpa);
}

void
apecs_mem_unmap(imfa, va, isasize)
	void *imfa;
	vm_offset_t va;
	isa_msize_t isasize;
{

	/* XXX sanity checks on va */

	/* Nothing to do; mapping was done in direct-mapped segment. */
}
