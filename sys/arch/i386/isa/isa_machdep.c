/*	$OpenBSD: isa_machdep.c,v 1.14 1996/05/07 07:22:17 deraadt Exp $	*/
/*	$NetBSD: isa_machdep.c,v 1.13 1996/05/03 19:14:55 christos Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)isa.c	7.2 (Berkeley) 5/13/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/icu.h>

#define	IDTVEC(name)	__CONCAT(X,name)
/* default interrupt vector table entries */
typedef (*vector) __P((void));
extern vector IDTVEC(intr)[], IDTVEC(fast)[];
extern struct gate_descriptor idt[];
void isa_strayintr __P((int));
void intr_calculatemasks __P((void));
int fakeintr __P((void *));

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
void
isa_defaultirq()
{
	int i;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		setgate(&idt[ICU_OFFSET + i], IDTVEC(intr)[i], 0, SDT_SYS386IGT,
		    SEL_KPL);
  
	/* initialize 8259's */
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU1+1, ICU_OFFSET);	/* starting at this vector index */
	outb(IO_ICU1+1, 1 << IRQ_SLAVE); /* slave on line 2 */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+1, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+1, 1);		/* 8086 mode */
#endif
	outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x68);		/* special mask mode (if available) */
	outb(IO_ICU1, 0x0a);		/* Read IRR by default. */
#ifdef REORDER_IRQ
	outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */
#endif

	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+1, ICU_OFFSET+8);	/* staring at this vector index */
	outb(IO_ICU2+1, IRQ_SLAVE);
#ifdef AUTO_EOI_2
	outb(IO_ICU2+1, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU2+1, 1);		/* 8086 mode */
#endif
	outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	outb(IO_ICU2, 0x68);		/* special mask mode (if available) */
	outb(IO_ICU2, 0x0a);		/* Read IRR by default. */
}

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi()
{

	log(LOG_CRIT, "NMI port 61 %x, port 70 %x\n", inb(0x61), inb(0x70));
	return(0);
}

u_long	intrstray[ICU_LEN] = {0};
/*
 * Caught a stray interrupt, notify
 */
void
isa_strayintr(irq)
	int irq;
{
        /*
         * Stray interrupts on irq 7 occur when an interrupt line is raised
         * and then lowered before the CPU acknowledges it.  This generally
         * means either the device is screwed or something is cli'ing too
         * long and it's timing out.
         */
	if (++intrstray[irq] <= 5)
		log(LOG_ERR, "stray interrupt %d%s\n", irq,
		    intrstray[irq] >= 5 ? "; stopped logging" : "");
}

int fastvec;
int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

/*
 * Recalculate the interrupt masks from scratch.
 * We could code special registry and deregistry versions of this function that
 * would be faster, but the code would be nastier, and we don't expect this to
 * happen very much anyway.
 */
void
intr_calculatemasks()
{
	int irq, level;
	struct intrhand *q;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		register int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			if (q->ih_level != IPL_NONE)
				levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < 5; level++) {
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		imask[level] = irqs | SIR_ALLMASK;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	imask[IPL_IMP] |= imask[IPL_TTY] | imask[IPL_NET] | imask[IPL_BIO];

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_TTY] |= imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_NET] |= imask[IPL_BIO];

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		register int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			if (q->ih_level != IPL_NONE)
				irqs |= imask[q->ih_level];
		intrmask[irq] = irqs | SIR_ALLMASK;
	}

	/* Lastly, determine which IRQs are actually in use. */
	{
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrhand[irq])
				irqs |= 1 << irq;
		if (irqs >= 0x100) /* any IRQs >= 8 in use */
			irqs |= 1 << IRQ_SLAVE;
		imen = ~irqs;
		SET_ICUS();
	}
}

int
fakeintr(arg)
	void *arg;
{

	return 0;
}

#define	LEGAL_IRQ(x)	((x) >= 0 && (x) < ICU_LEN && (x) != 2)

/*
 * Set up an interrupt handler to start being called.
 * XXX PRONE TO RACE CONDITIONS, UGLY, 'INTERESTING' INSERTION ALGORITHM.
 */
void *
isa_intr_establish(ic, irq, type, level, ih_fun, ih_arg, ih_what)
	isa_chipset_tag_t ic;
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
	char *ih_what;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {fakeintr};
	extern int cold;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("isa_intr_establish: can't malloc handler info");

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("intr_establish: bogus irq or type");

	switch (intrtype[irq]) {
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intrtype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    isa_intr_typename(intrtype[irq]),
			    isa_intr_typename(type));
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		;

	/*
	 * Actually install a fake handler momentarily, since we might be doing
	 * this with interrupts enabled and don't want the real routine called
	 * until masking is set up.
	 */
	fakehand.ih_level = level;
	*p = &fakehand;

	intr_calculatemasks();

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_count = 0;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	ih->ih_what = ih_what;
	*p = ih;

	return (ih);
}

/*
 * Deregister an interrupt handler.
 */
void
isa_intr_disestablish(ic, arg)
	isa_chipset_tag_t ic;
	void *arg;
{
	struct intrhand *ih = arg;
	int irq = ih->ih_irq;
	struct intrhand **p, *q;

	if (!LEGAL_IRQ(irq))
		panic("intr_disestablish: bogus irq");

	/*
	 * Remove the handler from the chain.
	 * This is O(n^2), too.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL && q != ih; p = &q->ih_next)
		;
	if (q)
		*p = q->ih_next;
	else
		panic("intr_disestablish: handler not registered");
	free(ih, M_DEVBUF);

	intr_calculatemasks();

	if (intrhand[irq] == NULL)
		intrtype[irq] = IST_NONE;
}

void
isa_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{

	/* Nothing to do. */
}

/*
 * ISA DMA and bounce buffer management
 */

#define MAX_CHUNK 256		/* number of low memory segments */

static unsigned long bitmap[MAX_CHUNK / 32 + 1];

#define set(i) (bitmap[(i) >> 5] |= (1 << (i)))
#define clr(i) (bitmap[(i) >> 5] &= ~(1 << (i)))
#define bit(i) ((bitmap[(i) >> 5] & (1 << (i))) != 0)

static int bit_ptr = -1;	/* last segment visited */
static int chunk_size = 0;	/* size (bytes) of one low mem segment */
static int chunk_num = 0;	/* actual number of low mem segments */
#ifdef DIAGNOSTIC
int bounce_alloc_cur = 0;
int bounce_alloc_max = 0;
#endif

vm_offset_t isaphysmem;		/* base address of low mem arena */
int isaphysmempgs;		/* number of pages of low mem arena */

/*
 * if addr is the physical address of an allocated bounce buffer return the
 * corresponding virtual address, 0 otherwise
 */

static caddr_t
bounce_vaddr(addr)
	vm_offset_t addr;
{
	int i;

	if (addr < vtophys(isaphysmem) ||
	    addr >= vtophys(isaphysmem + chunk_num*chunk_size) ||
	    ((i = (int)(addr-vtophys(isaphysmem))) % chunk_size) != 0 ||
	    bit(i/chunk_size))
		return(0);

	return((caddr_t) (isaphysmem + (addr - vtophys(isaphysmem))));
}

/*
 * alloc a low mem segment of size nbytes. Alignment constraint is:
 *   (addr & pmask) == ((addr+size-1) & pmask)
 * if waitok, call may wait for memory to become available.
 * returns 0 on failure
 */

static vm_offset_t
bounce_alloc(nbytes, pmask, waitok)
	vm_size_t nbytes;
	vm_offset_t pmask;
	int waitok;
{
	int i, l;
	vm_offset_t a, b, c, r;
	vm_size_t n;
	int nunits, opri;

	opri = splbio();

	if (bit_ptr < 0) {	/* initialize low mem arena */
		if ((chunk_size = isaphysmempgs*NBPG/MAX_CHUNK) & 1)
			chunk_size--;
		chunk_num =  (isaphysmempgs*NBPG) / chunk_size;
		for(i = 0; i < chunk_num; i++)
			set(i);
		bit_ptr = 0;
	}

	nunits = (nbytes+chunk_size-1)/chunk_size;

	/*
	 * set a=start, b=start with address constraints, c=end
	 * check if this request may ever succeed.
	 */

	a = isaphysmem;
	b = (isaphysmem + ~pmask) & pmask;
	c = isaphysmem + chunk_num*chunk_size;
	n = nunits*chunk_size;
	if (a + n >= c || pmask != 0 && a + n >= b && b + n >= c) {
		splx(opri);
		return(0);
	}

	for (;;) {
		i = bit_ptr;
		l = -1;
		do{
			if (bit(i) && l >= 0 && (i - l + 1) >= nunits){
				r = vtophys(isaphysmem + (i - nunits + 1)*chunk_size);
				if (((r ^ (r + nbytes - 1)) & pmask) == 0) {
					for (l = i - nunits + 1; l <= i; l++)
						clr(l);
					bit_ptr = i;
#ifdef DIAGNOSTIC
					bounce_alloc_cur += nunits*chunk_size;
					bounce_alloc_max = max(bounce_alloc_max,
							       bounce_alloc_cur);
#endif
					splx(opri);
					return(r);
				}
			} else if (bit(i) && l < 0)
				l = i;
			else if (!bit(i))
				l = -1;
			if (++i == chunk_num) {
				i = 0;
				l = -1;
			}
		} while(i != bit_ptr);

		if (waitok)
			tsleep((caddr_t) &bit_ptr, PRIBIO, "physmem", 0);
		else {
			splx(opri);
			return(0);
		}
	}
}

/* 
 * return a segent of the low mem arena to the free pool
 */

static void
bounce_free(addr, nbytes)
	vm_offset_t addr;
	vm_size_t nbytes;
{
	int i, j, opri;
	vm_offset_t vaddr;

	opri = splbio();

	if ((vaddr = (vm_offset_t) bounce_vaddr(addr)) == 0)
		panic("bounce_free: bad address");

	i = (int) (vaddr - isaphysmem)/chunk_size;
	j = i + (nbytes + chunk_size - 1)/chunk_size;

#ifdef DIAGNOSTIC
	bounce_alloc_cur -= (j - i)*chunk_size;
#endif

	while (i < j) {
		if (bit(i))
			panic("bounce_free: already free");
		set(i);
		i++;
	}

	wakeup((caddr_t) &bit_ptr);
	splx(opri);
}

/*
 * setup (addr, nbytes) for an ISA dma transfer.
 * flags&ISADMA_MAP_WAITOK	may wait
 * flags&ISADMA_MAP_BOUNCE	may use a bounce buffer if necessary
 * flags&ISADMA_MAP_CONTIG	result must be physically contiguous
 * flags&ISADMA_MAP_8BIT	must not cross 64k boundary
 * flags&ISADMA_MAP_16BIT	must not cross 128k boundary
 *
 * returns the number of used phys entries, 0 on failure.
 * if flags&ISADMA_MAP_CONTIG result is 1 on sucess!
 */

int
isadma_map(addr, nbytes, phys, flags)
	caddr_t addr;
	vm_size_t nbytes;
	struct isadma_seg *phys;
	int flags;
{
	vm_offset_t pmask, thiskv, thisphys, nextphys;
	vm_size_t datalen;
	int seg, waitok, i;

	if (flags & ISADMA_MAP_8BIT)
		pmask = ~((64*1024) - 1);
	else if (flags & ISADMA_MAP_16BIT)
		pmask = ~((128*1024) - 1);
	else
		pmask = 0;

	waitok = (flags & ISADMA_MAP_WAITOK) != 0;

	thiskv = (vm_offset_t) addr;
	datalen = nbytes;
	thisphys = vtophys(thiskv);
	seg = 0;

	while (datalen > 0 && (seg == 0 || (flags & ISADMA_MAP_CONTIG) == 0)) {
		phys[seg].length = 0;
		phys[seg].addr = thisphys;

		nextphys = thisphys;
		while (datalen > 0 && thisphys == nextphys) {
			nextphys = trunc_page(thisphys) + NBPG;
			phys[seg].length += min(nextphys - thisphys, datalen);
			datalen -= min(nextphys - thisphys, datalen);
			thiskv = trunc_page(thiskv) + NBPG;
			if (datalen)
				thisphys = vtophys(thiskv);
		}

		if (phys[seg].addr + phys[seg].length > 0xffffff) {
			if (flags & ISADMA_MAP_CONTIG) {
				phys[seg].length = nbytes;
				datalen = 0;
			}
			if ((flags & ISADMA_MAP_BOUNCE) == 0)
				phys[seg].addr = 0;
			else
				phys[seg].addr = bounce_alloc(phys[seg].length,
							      pmask, waitok);
			if (phys[seg].addr == 0) {
				for (i = 0; i < seg; i++)
					if (bounce_vaddr(phys[i].addr))
						bounce_free(phys[i].addr,
							    phys[i].length);
				return 0;
			}
		}

		seg++;
	}

	/* check all constraints */
	if (datalen ||
	    ((phys[0].addr ^ (phys[0].addr + phys[0].length - 1)) & pmask) != 0 ||
	    ((phys[0].addr & 1) && (flags & ISADMA_MAP_16BIT))) {
		if ((flags & ISADMA_MAP_BOUNCE) == 0)
			return 0;
		if ((phys[0].addr = bounce_alloc(nbytes, pmask, waitok)) == 0)
			return 0;
		phys[0].length = nbytes;
	}

	return seg;
}

/*
 * undo a ISA dma mapping. Simply return the bounced segments to the pool.
 */

void
isadma_unmap(addr, nbytes, nphys, phys)
	caddr_t addr;
	vm_size_t nbytes;
	int nphys;
	struct isadma_seg *phys;
{
	int i;

	for (i = 0; i < nphys; i++)
		if (bounce_vaddr(phys[i].addr))
			bounce_free(phys[i].addr, phys[i].length);
}

/*
 * copy bounce buffer to buffer where needed
 */

void
isadma_copyfrombuf(addr, nbytes, nphys, phys)
	caddr_t addr;
	vm_size_t nbytes;
	int nphys;
	struct isadma_seg *phys;
{
	int i;
	caddr_t vaddr;

	for (i = 0; i < nphys; i++) {
		if (vaddr = bounce_vaddr(phys[i].addr))
			bcopy(vaddr, addr, phys[i].length);
		addr += phys[i].length;
	}
}

/*
 * copy buffer to bounce buffer where needed
 */

void
isadma_copytobuf(addr, nbytes, nphys, phys)
	caddr_t addr;
	vm_size_t nbytes;
	int nphys;
	struct isadma_seg *phys;
{
	int i;
	caddr_t vaddr;

	for (i = 0; i < nphys; i++) {
		if (vaddr = bounce_vaddr(phys[i].addr))
			bcopy(addr, vaddr, phys[i].length);
		addr += phys[i].length;
	}
}
