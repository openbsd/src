/*	$OpenBSD: isa_machdep.c,v 1.32 1998/12/27 00:27:16 deraadt Exp $	*/
/*	$NetBSD: isa_machdep.c,v 1.22 1997/06/12 23:57:32 thorpej Exp $	*/

#define ISA_DMA_STATS

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993, 1994, 1996, 1997
 *	Charles M. Hannum.  All rights reserved.
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
#include <sys/proc.h>

#include <vm/vm.h>

#define _I386_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/icu.h>

#include <vm/vm.h>

#include "isadma.h"

/*
 * ISA can only DMA to 0-16M.
 */
#define	ISA_DMA_BOUNCE_THRESHOLD	0x00ffffff

extern	vm_offset_t avail_end;

#define	IDTVEC(name)	__CONCAT(X,name)
/* default interrupt vector table entries */
typedef int (*vector) __P((void));
extern vector IDTVEC(intr)[], IDTVEC(fast)[];
void isa_strayintr __P((int));
void intr_calculatemasks __P((void));
int fakeintr __P((void *));

#if NISADMA > 0
int	_isa_bus_dmamap_create __P((bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *));
void	_isa_bus_dmamap_destroy __P((bus_dma_tag_t, bus_dmamap_t));
int	_isa_bus_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	_isa_bus_dmamap_load_mbuf __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));
int	_isa_bus_dmamap_load_uio __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	_isa_bus_dmamap_load_raw __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
void	_isa_bus_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void	_isa_bus_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dmasync_op_t));

int	_isa_bus_dmamem_alloc __P((bus_dma_tag_t, bus_size_t, bus_size_t,
	    bus_size_t, bus_dma_segment_t *, int, int *, int));
void	_isa_bus_dmamem_free __P((bus_dma_tag_t,
	    bus_dma_segment_t *, int));
int	_isa_bus_dmamem_map __P((bus_dma_tag_t, bus_dma_segment_t *,
	    int, size_t, caddr_t *, int));
void	_isa_bus_dmamem_unmap __P((bus_dma_tag_t, caddr_t, size_t));
int	_isa_bus_dmamem_mmap __P((bus_dma_tag_t, bus_dma_segment_t *,
	    int, int, int, int));

int	_isa_dma_check_buffer __P((void *, bus_size_t, int, bus_size_t,
	    struct proc *));
int	_isa_dma_alloc_bouncebuf __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_size_t, int));
void	_isa_dma_free_bouncebuf __P((bus_dma_tag_t, bus_dmamap_t));

/*
 * Entry points for ISA DMA.  These are mostly wrappers around
 * the generic functions that understand how to deal with bounce
 * buffers, if necessary.
 */
struct i386_bus_dma_tag isa_bus_dma_tag = {
	NULL,			/* _cookie */
	_isa_bus_dmamap_create,
	_isa_bus_dmamap_destroy,
	_isa_bus_dmamap_load,
	_isa_bus_dmamap_load_mbuf,
	_isa_bus_dmamap_load_uio,
	_isa_bus_dmamap_load_raw,
	_isa_bus_dmamap_unload,
	_isa_bus_dmamap_sync,
	_isa_bus_dmamem_alloc,
	_isa_bus_dmamem_free,
	_isa_bus_dmamem_map,
	_isa_bus_dmamem_unmap,
	_isa_bus_dmamem_mmap,
};
#endif /* NISADMA > 0 */

/*
 * Fill in default interrupt table (in case of spurious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
void
isa_defaultirq()
{
	int i;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		setgate(&idt[ICU_OFFSET + i], IDTVEC(intr)[i], 0, SDT_SYS386IGT,
		    SEL_KPL, GICODE_SEL);
  
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

	/* This is historic garbage; these ports are not readable */
	log(LOG_CRIT, "No-maskable interrupt, may be parity error\n");
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

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0x00000000;
	imask[IPL_HIGH] = 0xffffffff;

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		register int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
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

int
isa_intr_alloc(ic, mask, type, irq)
	isa_chipset_tag_t ic;
	int mask;
	int type;
	int *irq;
{
	int i, bestirq, count;
	int tmp;
	struct intrhand **p, *q;

	if (type == IST_NONE)
		panic("intr_alloc: bogus type");

	bestirq = -1;
	count = -1;

	/* some interrupts should never be dynamically allocated */
	mask &= 0xdef8;

	/*
	 * XXX some interrupts will be used later (6 for fdc, 12 for pms).
	 * the right answer is to do "breadth-first" searching of devices.
	 */
	mask &= 0xefbf;

	for (i = 0; i < ICU_LEN; i++) {
		if (LEGAL_IRQ(i) == 0 || (mask & (1<<i)) == 0)
			continue;

		switch(intrtype[i]) {
		case IST_NONE:
			/*
			 * if nothing's using the irq, just return it
			 */
			*irq = i;
			return (0);

		case IST_EDGE:
		case IST_LEVEL:
			if (type != intrtype[i])
				continue;
			/*
			 * if the irq is shareable, count the number of other
			 * handlers, and if it's smaller than the last irq like
			 * this, remember it
			 *
			 * XXX We should probably also consider the
			 * interrupt level and stick IPL_TTY with other
			 * IPL_TTY, etc.
			 */
			for (p = &intrhand[i], tmp = 0; (q = *p) != NULL;
			     p = &q->ih_next, tmp++)
				;
			if ((bestirq == -1) || (count > tmp)) {
				bestirq = i;
				count = tmp;
			}
			break;

		case IST_PULSE:
			/* this just isn't shareable */
			continue;
		}
	}

	if (bestirq == -1)
		return (1);

	*irq = bestirq;

	return (0);
}

/*
 * Just check to see if an IRQ is available/can be shared.
 * 0 = no match, 1 = ok match, 2 = great match
 */
int
isa_intr_check(ic, irq, type)
	isa_chipset_tag_t ic;	/* Not used. */
	int irq;
	int type;
{
	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		return (0);

	switch (intrtype[irq]) {
	case IST_NONE:
		return (2);
		break;
	case IST_LEVEL:
		if (type != intrtype[irq])
			return (0);
		return (2);
		break;
	case IST_EDGE:
	case IST_PULSE:
		if (type != IST_NONE)
			return (0);
	}
	return (1);
}

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
	if (ih == NULL) {
		printf("%s: isa_intr_establish: can't malloc handler info\n",
		    ih_what);
		return NULL;
	}

	if (!LEGAL_IRQ(irq) || type == IST_NONE) {
		printf("%s: intr_establish: bogus irq or type\n", ih_what);
		return NULL;
	}
	switch (intrtype[irq]) {
	case IST_NONE:
		intrtype[irq] = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intrtype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE) {
			/*printf("%s: intr_establish: can't share %s with %s, irq %d\n",
			    ih_what, isa_intr_typename(intrtype[irq]),
			    isa_intr_typename(type), irq);*/
			return NULL;
		}
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
	extern int isa_has_been_seen;

	/*
	 * Notify others that might need to know that the ISA bus
	 * has now been attached.
	 */
	if (isa_has_been_seen)
		panic("isaattach: ISA bus already seen!");
	isa_has_been_seen = 1;
}

#if NISADMA > 0
/**********************************************************************
 * bus.h dma interface entry points
 **********************************************************************/

#ifdef ISA_DMA_STATS
#define	STAT_INCR(v)	(v)++
#define	STAT_DECR(v)	do { \
		if ((v) == 0) \
			printf("%s:%d -- Already 0!\n", __FILE__, __LINE__); \
		else \
			(v)--; \
		} while (0)
u_long	isa_dma_stats_loads;
u_long	isa_dma_stats_bounces;
u_long	isa_dma_stats_nbouncebufs;
#else
#define	STAT_INCR(v)
#define	STAT_DECR(v)
#endif

/*
 * Create an ISA DMA map.
 */
int
_isa_bus_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct i386_isa_dma_cookie *cookie;
	bus_dmamap_t map;
	int error, cookieflags;
	void *cookiestore;
	size_t cookiesize;

	/* Call common function to create the basic map. */
	error = _bus_dmamap_create(t, size, nsegments, maxsegsz, boundary,
	    flags, dmamp);
	if (error)
		return (error);

	map = *dmamp;
	map->_dm_cookie = NULL;

	cookiesize = sizeof(struct i386_isa_dma_cookie);

	/*
	 * ISA only has 24-bits of address space.  This means
	 * we can't DMA to pages over 16M.  In order to DMA to
	 * arbitrary buffers, we use "bounce buffers" - pages
	 * in memory below the 16M boundary.  On DMA reads,
	 * DMA happens to the bounce buffers, and is copied into
	 * the caller's buffer.  On writes, data is copied into
	 * but bounce buffer, and the DMA happens from those
	 * pages.  To software using the DMA mapping interface,
	 * this looks simply like a data cache.
	 *
	 * If we have more than 16M of RAM in the system, we may
	 * need bounce buffers.  We check and remember that here.
	 *
	 * There are exceptions, however.  VLB devices can do
	 * 32-bit DMA, and indicate that here.
	 *
	 * ...or, there is an opposite case.  The most segments
	 * a transfer will require is (maxxfer / NBPG) + 1.  If
	 * the caller can't handle that many segments (e.g. the
	 * ISA DMA controller), we may have to bounce it as well.
	 */
	cookieflags = 0;
	if ((avail_end > ISA_DMA_BOUNCE_THRESHOLD &&
	    (flags & ISABUS_DMA_32BIT) == 0) ||
	    ((map->_dm_size / NBPG) + 1) > map->_dm_segcnt) {
		cookieflags |= ID_MIGHT_NEED_BOUNCE;
		cookiesize += (sizeof(bus_dma_segment_t) * map->_dm_segcnt);
	}

	/*
	 * Allocate our cookie.
	 */
	if ((cookiestore = malloc(cookiesize, M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL) {
		error = ENOMEM;
		goto out;
	}
	bzero(cookiestore, cookiesize);
	cookie = (struct i386_isa_dma_cookie *)cookiestore;
	cookie->id_flags = cookieflags;
	map->_dm_cookie = cookie;

	if (cookieflags & ID_MIGHT_NEED_BOUNCE) {
		/*
		 * Allocate the bounce pages now if the caller
		 * wishes us to do so.
		 */
		if ((flags & BUS_DMA_ALLOCNOW) == 0)
			goto out;

		error = _isa_dma_alloc_bouncebuf(t, map, size, flags);
	}

 out:
	if (error) {
		if (map->_dm_cookie != NULL)
			free(map->_dm_cookie, M_DEVBUF);
		_bus_dmamap_destroy(t, map);
	}
	return (error);
}

/*
 * Destroy an ISA DMA map.
 */
void
_isa_bus_dmamap_destroy(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;

	/*
	 * Free any bounce pages this map might hold.
	 */
	if (cookie->id_flags & ID_HAS_BOUNCE)
		_isa_dma_free_bouncebuf(t, map);

	free(cookie, M_DEVBUF);
	_bus_dmamap_destroy(t, map);
}

/*
 * Load an ISA DMA map with a linear buffer.
 */
int
_isa_bus_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map; 
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;
	int error;

	STAT_INCR(isa_dma_stats_loads);

	/*
	 * Check to see if we might need to bounce the transfer.
	 */
	if (cookie->id_flags & ID_MIGHT_NEED_BOUNCE) {
		/*
		 * Check if all pages are below the bounce
		 * threshold.  If they are, don't bother bouncing.
		 */
		if (_isa_dma_check_buffer(buf, buflen,
		    map->_dm_segcnt, map->_dm_boundary, p) == 0)
			return (_bus_dmamap_load(t, map, buf, buflen,
			    p, flags));

		STAT_INCR(isa_dma_stats_bounces);

		/*
		 * Allocate bounce pages, if necessary.
		 */
		if ((cookie->id_flags & ID_HAS_BOUNCE) == 0) {
			error = _isa_dma_alloc_bouncebuf(t, map, buflen,
			    flags);
			if (error)
				return (error);
		}

		/*
		 * Cache a pointer to the caller's buffer and
		 * load the DMA map with the bounce buffer.
		 */
		cookie->id_origbuf = buf;
		cookie->id_origbuflen = buflen;
		error = _bus_dmamap_load(t, map, cookie->id_bouncebuf,
		    buflen, p, flags);
		
		if (error) {
			/*
			 * Free the bounce pages, unless our resources
			 * are reserved for our exclusive use.
			 */
			if ((map->_dm_flags & BUS_DMA_ALLOCNOW) == 0)
				_isa_dma_free_bouncebuf(t, map);
		}

		/* ...so _isa_bus_dmamap_sync() knows we're bouncing */
		cookie->id_flags |= ID_IS_BOUNCING;
	} else {
		/*
		 * Just use the generic load function.
		 */
		error = _bus_dmamap_load(t, map, buf, buflen, p, flags); 
	}

	return (error);
}

/*
 * Like _isa_bus_dmamap_load(), but for mbufs.
 */
int
_isa_bus_dmamap_load_mbuf(t, map, m, flags)  
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	panic("_isa_bus_dmamap_load_mbuf: not implemented");
}

/*
 * Like _isa_bus_dmamap_load(), but for uios.
 */
int
_isa_bus_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	panic("_isa_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _isa_bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_isa_bus_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	panic("_isa_bus_dmamap_load_raw: not implemented");
}

/*
 * Unload an ISA DMA map.
 */
void
_isa_bus_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;

	/*
	 * If we have bounce pages, free them, unless they're
	 * reserved for our exclusive use.
	 */
	if ((cookie->id_flags & ID_HAS_BOUNCE) &&
	    (map->_dm_flags & BUS_DMA_ALLOCNOW) == 0)
		_isa_dma_free_bouncebuf(t, map);

	cookie->id_flags &= ~ID_IS_BOUNCING;

	/*
	 * Do the generic bits of the unload.
	 */
	_bus_dmamap_unload(t, map);
}

/*
 * Synchronize an ISA DMA map.
 */
void
_isa_bus_dmamap_sync(t, map, op)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dmasync_op_t op;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;

	switch (op) {
	case BUS_DMASYNC_PREREAD:
		/*
		 * Nothing to do for pre-read.
		 */
		break;

	case BUS_DMASYNC_PREWRITE:
		/*
		 * If we're bouncing this transfer, copy the
		 * caller's buffer to the bounce buffer.
		 */
		if (cookie->id_flags & ID_IS_BOUNCING)
			bcopy(cookie->id_origbuf, cookie->id_bouncebuf,
			    cookie->id_origbuflen);
		break;

	case BUS_DMASYNC_POSTREAD:
		/*
		 * If we're bouncing this transfer, copy the
		 * bounce buffer to the caller's buffer.
		 */
		if (cookie->id_flags & ID_IS_BOUNCING)
			bcopy(cookie->id_bouncebuf, cookie->id_origbuf,
			    cookie->id_origbuflen);
		break;

	case BUS_DMASYNC_POSTWRITE:
		/*
		 * Nothing to do for post-write.
		 */
		break;
	}

#if 0
	/* This is a noop anyhow, so why bother calling it? */
	_bus_dmamap_sync(t, map, op);
#endif
}

/*
 * Allocate memory safe for ISA DMA.
 */
int
_isa_bus_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vm_offset_t high;

	if (avail_end > ISA_DMA_BOUNCE_THRESHOLD)
		high = trunc_page(ISA_DMA_BOUNCE_THRESHOLD);
	else
		high = trunc_page(avail_end);

	return (_bus_dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, 0, high));
}

/*
 * Free memory safe for ISA DMA.
 */
void
_isa_bus_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{

	_bus_dmamem_free(t, segs, nsegs);
}

/*
 * Map ISA DMA-safe memory into kernel virtual address space.
 */
int
_isa_bus_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{

	return (_bus_dmamem_map(t, segs, nsegs, size, kvap, flags));
}

/*
 * Unmap ISA DMA-safe memory from kernel virtual address space.
 */
void
_isa_bus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{

	_bus_dmamem_unmap(t, kva, size);
}

/*
 * mmap(2) ISA DMA-safe memory.
 */
int
_isa_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{

	return (_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags));
}

/**********************************************************************
 * ISA DMA utility functions
 **********************************************************************/

/*
 * Return 0 if all pages in the passed buffer lie within the DMA'able
 * range RAM.
 */
int
_isa_dma_check_buffer(buf, buflen, segcnt, boundary, p)
	void *buf;
	bus_size_t buflen;
	int segcnt;
	bus_size_t boundary;
	struct proc *p;
{
	vm_offset_t vaddr = (vm_offset_t)buf;
	vm_offset_t pa, lastpa, endva;
	u_long pagemask = ~(boundary - 1);
	pmap_t pmap;
	int nsegs;

	endva = round_page(vaddr + buflen);

	nsegs = 1;
	lastpa = 0;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	for (; vaddr < endva; vaddr += NBPG) {
		/*
		 * Get physical address for this segment.
		 */
		pa = pmap_extract(pmap, (vm_offset_t)vaddr);
		pa = trunc_page(pa);

		/*
		 * Is it below the DMA'able threshold?
		 */
		if (pa > ISA_DMA_BOUNCE_THRESHOLD)
			return (EINVAL);

		if (lastpa) {
			/*
			 * Check excessive segment count.
			 */
			if (lastpa + NBPG != pa) {
				if (++nsegs > segcnt)
					return (EFBIG);
			}

			/*
			 * Check boundary restriction.
			 */
			if (boundary) {
				if ((lastpa ^ pa) & pagemask)
					return (EINVAL);
			}
		}
		lastpa = pa;
	}

	return (0);
}

int
_isa_dma_alloc_bouncebuf(t, map, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_size_t size;
	int flags;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;
	int error = 0;

	cookie->id_bouncebuflen = round_page(size);
	error = _isa_bus_dmamem_alloc(t, cookie->id_bouncebuflen,
	    NBPG, map->_dm_boundary, cookie->id_bouncesegs,
	    map->_dm_segcnt, &cookie->id_nbouncesegs, flags);
	if (error)
		goto out;
	error = _isa_bus_dmamem_map(t, cookie->id_bouncesegs,
	    cookie->id_nbouncesegs, cookie->id_bouncebuflen,
	    (caddr_t *)&cookie->id_bouncebuf, flags);

 out:
	if (error) {
		_isa_bus_dmamem_free(t, cookie->id_bouncesegs,
		    cookie->id_nbouncesegs);
		cookie->id_bouncebuflen = 0;
		cookie->id_nbouncesegs = 0;
	} else {
		cookie->id_flags |= ID_HAS_BOUNCE;
		STAT_INCR(isa_dma_stats_nbouncebufs);
	}

	return (error);
}

void
_isa_dma_free_bouncebuf(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct i386_isa_dma_cookie *cookie = map->_dm_cookie;

	STAT_DECR(isa_dma_stats_nbouncebufs);

	_isa_bus_dmamem_unmap(t, cookie->id_bouncebuf,
	    cookie->id_bouncebuflen);
	_isa_bus_dmamem_free(t, cookie->id_bouncesegs,
	    cookie->id_nbouncesegs);
	cookie->id_bouncebuflen = 0;
	cookie->id_nbouncesegs = 0;
	cookie->id_flags &= ~ID_HAS_BOUNCE;
}

#ifdef __ISADMA_COMPAT
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
	bus_dma_tag_t dmat = ((struct isa_softc *)isa_dev)->sc_dmat;
	bus_dmamap_t dmam;
	int i;

/* XXX if this turns out to be too low, convert the driver to real bus_dma */
#define ISADMA_MAX_SEGMENTS 64
#define ISADMA_MAX_SEGSZ 0xffffff

	if (bus_dmamap_create(dmat, nbytes,
	    (flags & ISADMA_MAP_CONTIG) ? 1 : ISADMA_MAX_SEGMENTS,
	    ISADMA_MAX_SEGSZ,
	    (flags & ISADMA_MAP_8BIT) ? 0xffff :
	    ((flags & ISADMA_MAP_16BIT) ? 0x1ffff : 0),
	    (flags & ISADMA_MAP_WAITOK) ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT,
	    &dmam) != 0)
		return (0);
	if (bus_dmamap_load(dmat, dmam, addr, nbytes, 0,
	    (flags & ISADMA_MAP_WAITOK) ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT) !=
	     0) {
		bus_dmamap_destroy(dmat, dmam);
		return (0);
	}
	for (i = 0; i < dmam->dm_nsegs; i++) {
		phys[i].addr = dmam->dm_segs[i].ds_addr;
		phys[i].length = dmam->dm_segs[i].ds_len;
	}
	phys[0].dmam = dmam;
	return (dmam->dm_nsegs);
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
	bus_dma_tag_t dmat = ((struct isa_softc *)isa_dev)->sc_dmat;
	bus_dmamap_t dmam = phys[0].dmam;

	if (dmam == NULL)
		return;
	bus_dmamap_unload(dmat, dmam);
	bus_dmamap_destroy(dmat, dmam);
	phys[0].dmam = NULL;
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
	bus_dma_tag_t dmat = ((struct isa_softc *)isa_dev)->sc_dmat;
	bus_dmamap_t dmam = phys[0].dmam;

	bus_dmamap_sync(dmat, dmam, BUS_DMASYNC_POSTREAD);
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
	bus_dma_tag_t dmat = ((struct isa_softc *)isa_dev)->sc_dmat;
	bus_dmamap_t dmam = phys[0].dmam;

	bus_dmamap_sync(dmat, dmam, BUS_DMASYNC_PREWRITE);
}
#endif /* __ISADMA_COMPAT */
#endif /* NISADMA > 0 */
