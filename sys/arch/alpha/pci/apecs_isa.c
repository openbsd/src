/*	$NetBSD: apecs_isa.c,v 1.3 1995/08/03 01:16:53 cgd Exp $	*/

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
#include <vm/vm.h>

#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isadmavar.h>
#include <alpha/isa/isa_dma.h>

#include <alpha/pci/apecsreg.h>

static u_int8_t		apecs_inb __P((int port));
/* static void		apecs_insb __P((int port, void *addr, int cnt)); */
#define	apecs_insb	0					/* XXX */
static u_int16_t	apecs_inw __P((int port));
/* static void		apecs_insw __P((int port, void *addr, int cnt)); */
#define	apecs_insw	0					/* XXX */
u_int32_t		apecs_inl __P((int port));
/* static void		apecs_insl __P((int port, void *addr, int cnt)); */
#define	apecs_insl	0					/* XXX */

static void		apecs_outb __P((int port, u_int8_t datum));
/* static void		apecs_outsb __P((int port, void *addr, int cnt)); */
#define	apecs_outsb	0					/* XXX */
static void		apecs_outw __P((int port, u_int16_t datum));
/* static void		apecs_outsw __P((int port, void *addr, int cnt)); */
#define	apecs_outsw	0					/* XXX */
static void		apecs_outl __P((int port, u_int32_t datum));
/* static void		apecs_outsl __P((int port, void *addr, int cnt)); */
#define	apecs_outsl	0					/* XXX */

struct isa_pio_fcns apecs_pio_fcns = {
	apecs_inb,	apecs_insb,
	apecs_inw,	apecs_insw,
	apecs_inl,	apecs_insl,
	apecs_outb,	apecs_outsb,
	apecs_outw,	apecs_outsw,
	apecs_outl,	apecs_outsl,
};

static int	apecs_isadma_map __P((caddr_t addr, vm_size_t size,
		    vm_offset_t *mappings, int flags));
static void	apecs_isadma_unmap __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));
static void	apecs_isadma_copytobuf __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));
static void	apecs_isadma_copyfrombuf __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));

struct isadma_fcns apecs_isadma_fcns = {
	apecs_isadma_map,	apecs_isadma_unmap,
	apecs_isadma_copytobuf,	apecs_isadma_copyfrombuf,
};

u_int8_t
apecs_inb(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int8_t rval;
	int offset;

	wbflush();
	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 0 << 3 | ioaddr << 5);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;
/*	rval = val & 0xff; */

	return rval;
}

u_int16_t
apecs_inw(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int16_t rval;
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

u_int32_t
apecs_inl(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int32_t rval;
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
apecs_outb(ioaddr, val)
	int ioaddr;
	u_int8_t val;
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

void
apecs_outw(ioaddr, val)
	int ioaddr;
	u_int16_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 1 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

void
apecs_outl(ioaddr, val)
	int ioaddr;
	u_int32_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(APECS_PCI_SIO | 3 << 3 | ioaddr << 5);

	*port = nval;
	wbflush();
}

static caddr_t bounced_addr;
static caddr_t bounce_buffer;
static vm_size_t bounce_size;

int
apecs_isadma_map(addr, size, mappings, flags)
	caddr_t addr;
	vm_size_t size;
	vm_offset_t *mappings;
	int flags;
{
	vm_offset_t off, truncaddr;
	vm_offset_t isa_truncpa;			/* XXX? */
	vm_size_t alignment;
	int i, npages, waitok;

	/*
	 * ISADMA_MAP_{,NO}BOUNCE and ISADMA_MAP_{CONTIG,SCATTER} are
	 * completely ignored, because all allocations will be in the
	 * low 16M and will be contiguous.  I LOVE VIRTUAL DMA!
	 */

	truncaddr = trunc_page(addr);
	off = (vm_offset_t)addr - truncaddr;
	npages = num_pages(size + off);
	if (npages == 0)
		panic("apecs_isadma_map: map nothing");

	alignment = 64 * 1024;
	if ((flags & ISADMA_MAP_16BIT) != 0)
		alignment <<= 1;
	waitok = (flags & ISADMA_MAP_WAITOK) != 0;

	if (npages > atop(alignment)) {
		int s;
		void *tmpbb;

		/*
		 * Allocate a bounce buffer.
		 */
		s = splhigh();
retry:
		while (bounce_buffer != NULL) {
			/*
			 * If a bounce buffer is in use and we can't
			 * wait, bug out now, otherwise sleep.
			 */
			if (!waitok) {
				splx(s);
				return 0;
			}

			tsleep(&bounce_buffer, PRIBIO+1, "apecsbb", 0);
		}

		/*
		 * Try to allocate a bounce buffer.
		 */
		tmpbb = malloc(alignment, M_DEVBUF,
		    waitok ? M_WAITOK : M_NOWAIT);
		if (tmpbb == NULL) {	/* couldn't wait, and failed */
			splx(s);
			return 0;
		}

		/*
		 * If we slept in malloc() and somebody else got it,
		 * give it up and try it again!
		 */
		if (bounce_buffer != NULL) {
			free(tmpbb, M_DEVBUF);
			goto retry;
		}

		/*
		 * It's ours, all ours!
		 */
		bounce_buffer = tmpbb;
		splx(s);

		bounced_addr = addr;
		bounce_size = size;
		truncaddr = (vm_offset_t)bounce_buffer;
		npages = atop(alignment);
	}

	isa_truncpa = apecs_sgmap_alloc(truncaddr, npages, alignment, waitok);

	mappings[0] = isa_truncpa + off;
	for (i = 1; i < npages; i++)
		mappings[i] = isa_truncpa + ptoa(i);

	return (npages);
}

void
apecs_isadma_unmap(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{
	int npages;

	npages = nmappings;
	if (npages == 0)
		panic("apecs_isadma_unmap: unmap nothing");
	apecs_sgmap_dealloc(trunc_page(mappings[0]), npages);

	if (addr == bounced_addr) {
		/*
		 * Free the bounce buffer and wake up anybody who
		 * wants to bounce.
		 */
		bounced_addr = NULL;
		bounce_size = 0;
		free(bounce_buffer, M_DEVBUF);
		bounce_buffer = NULL;
		wakeup(&bounce_buffer);
	}
}

void
apecs_isadma_copytobuf(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	if (addr != bounced_addr)
		return;

	log(LOG_NOTICE, "apecs_isa_copytobuf: copied %d byte buffer\n",
	    bounce_size);
	bcopy(addr, bounce_buffer, bounce_size);
}

void
apecs_isadma_copyfrombuf(addr, size, nmappings, mappings)
	caddr_t addr;
	vm_size_t size;
	int nmappings;
	vm_offset_t *mappings;
{

	if (addr != bounced_addr)
		return;

	log(LOG_NOTICE, "apecs_isa_copyfrombuf: copied %d byte buffer\n",
	    bounce_size);
	bcopy(bounce_buffer, addr, bounce_size);
}
