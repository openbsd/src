/*	$NetBSD: cia_isa.c,v 1.1 1995/11/23 02:37:26 cgd Exp $	*/

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

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

/*
 * Allocation/deallocation functions.
 */
int	cia_pio_alloc __P((void *, isa_iooffset_t, isa_iosize_t));
int	cia_pio_dealloc __P((void *, isa_iooffset_t, isa_iosize_t));

/*
 * Byte functions.
 */
isa_byte_t	cia_inb __P((void *, isa_iooffset_t));
#define	cia_insb	0					/* XXX */
void		cia_outb __P((void *, isa_iooffset_t, isa_byte_t));
#define	cia_outsb	0					/* XXX */

/*
 * Word functions.
 */
isa_word_t	cia_inw __P((void *, isa_iooffset_t));
#define	cia_insw	0					/* XXX */
void		cia_outw __P((void *, isa_iooffset_t, isa_word_t));
#define	cia_outsw	0					/* XXX */

/*
 * Longword functions.
 */
isa_long_t	cia_inl __P((void *, isa_iooffset_t));
#define	cia_insl	0					/* XXX */
void		cia_outl __P((void *, isa_iooffset_t, isa_long_t));
#define	cia_outsl	0					/* XXX */

__const struct pci_pio_fns cia_pio_fns = {
	/* Allocation/deallocation functions. */
	cia_pio_alloc,	cia_pio_dealloc,

	/* Byte functions. */
	cia_inb,	cia_insb,
	cia_outb,	cia_outsb,

	/* Word functions. */
	cia_inw,	cia_insw,
	cia_outw,	cia_outsw,

	/* Longword functions. */
	cia_inl,	cia_insl,
	cia_outl,	cia_outsl,
};

int
cia_pio_alloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

int
cia_pio_dealloc(ipfarg, start, size)
	void *ipfarg;
	isa_iooffset_t start;
	isa_iosize_t size;
{

	/* XXX should do something */
}

isa_byte_t
cia_inb(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_byte_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 0 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;
/*	rval = val & 0xff; */

	return rval;
}

void
cia_outb(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_byte_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	nval = val << (8 * offset);
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 0 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_word_t
cia_inw(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_word_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 1 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;
	rval = val & 0xffff;

panic("inw(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

void
cia_outw(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_word_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 1 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

isa_long_t
cia_inl(ipfa, ioaddr)
	void *ipfa;
	isa_iooffset_t ioaddr;
{
	u_int32_t *port, val;
	isa_long_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 3 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffffffff;
	rval = val & 0xffffffff;

	return rval;
}

void
cia_outl(ipfa, ioaddr, val)
	void *ipfa;
	isa_iooffset_t ioaddr;
	isa_long_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(CIA_PCI_SIO0 | 3 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

/* XXX XXX XXX */

#define pf(fn, args)	fn args { panic(__STRING(fn)); }

void	pf(cia_dma_cascade, (void *idfa, isa_drq_t chan))
void	pf(cia_dma_copytobuf, ())
void	pf(cia_dma_copyfrombuf, ())
void	pf(cia_dma_start, (void *idfa, vm_offset_t addr,
	    isa_msize_t size, isa_drq_t chan, int flags))
void	pf(cia_dma_abort, (void *idfa, isa_drq_t chan))
void	pf(cia_dma_done, (void *idfa, isa_drq_t chan))

int	cia_dma_map __P((void *, vm_offset_t, isa_msize_t,
	    isa_moffset_t *, int));
void	cia_dma_unmap __P((void *, vm_offset_t, isa_msize_t, int,
	    isa_moffset_t *));

__const struct isa_dma_fns cia_dma_fns = {
	cia_dma_cascade,
	cia_dma_map,
	cia_dma_unmap,
	cia_dma_copytobuf,
	cia_dma_copyfrombuf,
	cia_dma_start,
	cia_dma_abort,
	cia_dma_done,
};

int
cia_dma_map(idfa, va, isasize, mappingsp, flags)
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
		panic("cia_dma_map: punt");

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
cia_dma_unmap(idfa, va, isasize, nmappings, mappingsp)
	void *idfa;
	vm_offset_t va;
	isa_msize_t isasize;
	int nmappings;
	isa_moffset_t *mappingsp;
{

	printf("cia_dma_unmap: called\n");
}

vm_offset_t	cia_mem_map __P((void *, isa_moffset_t, isa_msize_t, int));
void		cia_mem_unmap __P((void *, vm_offset_t, isa_msize_t));

#if 0
void		cia_mem_copytoisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		cia_mem_copyfromisa __P((void *, char *, vm_offset_t,
		    isa_moffset_t, isa_msize_t));
void		cia_mem_zero __P((void *, vm_offset_t, isa_moffset_t,
		    isa_msize_t));
#else
void		pf(cia_mem_copytoisa, ())
void		pf(cia_mem_copyfromisa, ())
void		pf(cia_mem_zero, ())
#endif

__const struct isa_mem_fns cia_mem_fns = {
	cia_mem_map,
	cia_mem_unmap,
	cia_mem_copytoisa,
	cia_mem_copyfromisa,
	cia_mem_zero,
};

vm_offset_t
cia_mem_map(imfa, isapa, isasize, cacheable)
	void *imfa;
	isa_moffset_t isapa;
	isa_msize_t isasize;
	int cacheable;
{
	vm_offset_t sbpa;

	/* XXX sanity checks on sizes, use of windows, etc. */

	/* XXX MAGIC NUMBERS */
	if (cacheable)
		sbpa = (isapa & 0xffffffff) | CIA_PCI_DENSE;
	else
		sbpa = ((isapa & 0x7ffffff) << 5) | CIA_PCI_SPARSE0;

	return phystok0seg(sbpa);
}

void
cia_mem_unmap(imfa, va, isasize)
	void *imfa;
	vm_offset_t va;
	isa_msize_t isasize;
{

	/* XXX sanity checks on va */

	/* Nothing to do; mapping was done in direct-mapped segment. */
}
