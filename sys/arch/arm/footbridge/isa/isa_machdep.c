/*	$OpenBSD: isa_machdep.c,v 1.4 2004/08/17 19:40:45 drahn Exp $	*/
/*	$NetBSD: isa_machdep.c,v 1.4 2003/06/16 20:00:57 thorpej Exp $	*/

/*-
 * Copyright (c) 1996-1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mark Brinicombe, Charles M. Hannum and by Jason R. Thorpe of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#define _ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/bootconfig.h>
#include <machine/isa_machdep.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmareg.h>
#include <dev/isa/isadmavar.h>
#include <arm/footbridge/isa/icu.h>
#include <arm/footbridge/dc21285reg.h>
#include <arm/footbridge/dc21285mem.h>

#include <uvm/uvm_extern.h>

#include "isadma.h"

/* prototypes */
static void isa_icu_init (void);

struct arm32_isa_chipset isa_chipset_tag;

void isa_strayintr (int);
void intr_calculatemasks (void);
int fakeintr (void *);

int isa_irqdispatch (void *arg);

u_int imask[NIPL];
unsigned imen;

#define AUTO_EOI_1
#define AUTO_EOI_2

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
static void
isa_icu_init(void)
{
	/* initialize 8259's */
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU1+1, ICU_OFFSET);	/* starting at this vector index */
	outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+1, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+1, 1);			/* 8086 mode */
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
	outb(IO_ICU2+1, 1);			/* 8086 mode */
#endif
	outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	outb(IO_ICU2, 0x68);		/* special mask mode (if available) */
	outb(IO_ICU2, 0x0a);		/* Read IRR by default. */
}

/*
 * Caught a stray interrupt, notify
 */
void
isa_strayintr(irq)
	int irq;
{
	static u_long strays;

        /*
         * Stray interrupts on irq 7 occur when an interrupt line is raised
         * and then lowered before the CPU acknowledges it.  This generally
         * means either the device is screwed or something is cli'ing too
         * long and it's timing out.
         */
	if (++strays <= 5)
		log(LOG_ERR, "stray interrupt %d%s\n", irq,
		    strays >= 5 ? "; stopped logging" : "");
}

static struct intrq isa_intrq[ICU_LEN];

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
	struct intrq *iq;
	struct intrhand *ih;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		int levels = 0;
		iq = &isa_intrq[irq];
		for (ih = TAILQ_FIRST(&iq->iq_list); ih != NULL;
			ih = TAILQ_NEXT(ih, ih_list))
			levels |= (1U << ih->ih_ipl);
		iq->iq_levels = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < NIPL; level++) {
		int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (isa_intrq[irq].iq_levels & (1U << level))
				irqs |= (1U << irq);
		imask[level] = irqs;
	}

	/*
	 * IPL_NONE is used for hardware interrupts that are never blocked,
	 * and do not block anything else.
	 */
	imask[IPL_NONE] = 0;

	imask[IPL_SOFT] |= imask[IPL_NONE];
	imask[IPL_SOFTCLOCK] |= imask[IPL_SOFT];
	imask[IPL_SOFTNET] |= imask[IPL_SOFTCLOCK];

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_BIO] |= imask[IPL_SOFTCLOCK];
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_SOFTSERIAL] |= imask[IPL_NET];
	imask[IPL_TTY] |= imask[IPL_NET];
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	imask[IPL_VM] |= imask[IPL_TTY];
	imask[IPL_AUDIO] |= imask[IPL_VM];

	/*
	 * Since run queues may be manipulated by both the statclock and tty,
	 * network, and disk drivers, clock > imp.
	 */
	imask[IPL_CLOCK] |= imask[IPL_VM];
	imask[IPL_STATCLOCK] |= imask[IPL_CLOCK];

	/*
	 * IPL_HIGH must block everything that can manipulate a run queue.
	 */
	imask[IPL_HIGH] |= imask[IPL_STATCLOCK];

	/*
	 * We need serial drivers to run at the absolute highest priority to
	 * avoid overruns, so serial > high.
	 */
	imask[IPL_SERIAL] |= imask[IPL_HIGH];

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		int irqs = 1 << irq;
		iq = &isa_intrq[irq];
		for (ih = TAILQ_FIRST(&iq->iq_list); ih != NULL;
			ih = TAILQ_NEXT(ih, ih_list))
			irqs |= imask[ih->ih_ipl];
		iq->iq_mask = irqs;
	}

	/* Lastly, determine which IRQs are actually in use. */
	{
		int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (!TAILQ_EMPTY(&isa_intrq[irq].iq_list))
				irqs |= (1U << irq);
		if (irqs >= 0x100) /* any IRQs >= 8 in use */
			irqs |= 1 << IRQ_SLAVE;
		imen = ~irqs;
		SET_ICUS();
	}
#if 0
	printf("type\tmask\tlevel\thand\n");
	for (irq = 0; irq < ICU_LEN; irq++) {
		printf("%x\t%04x\t%x\t%p\n", intrtype[irq], intrmask[irq],
		intrlevel[irq], intrhand[irq]);
	}
	for (level = 0; level < IPL_LEVELS; ++level)
		printf("%d: %08x\n", level, imask[level]);
#endif
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
	int i, tmp, bestirq, count;
	struct intrq *iq;
	struct intrhand *ih;

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
		
		iq = &isa_intrq[i];
		switch(iq->iq_ist) {
		case IST_NONE:
			/*
			 * if nothing's using the irq, just return it
			 */
			*irq = i;
			return (0);

		case IST_EDGE:
		case IST_LEVEL:
			if (type != iq->iq_ist)
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
			tmp = 0;
			TAILQ_FOREACH(ih, &(iq->iq_list), ih_list)
			     tmp++;
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
 * Set up an interrupt handler to start being called.
 * XXX PRONE TO RACE CONDITIONS, UGLY, 'INTERESTING' INSERTION ALGORITHM.
 */
void *
isa_intr_establish(ic, irq, type, level, ih_fun, ih_arg, name)
	isa_chipset_tag_t ic;
	int irq;
	int type;
	int level;
	int (*ih_fun) (void *);
	void *ih_arg;
	char *name;
{
    	struct intrq *iq;
	struct intrhand *ih;
	u_int oldirqstate;

#if 0
	printf("isa_intr_establish(%d, %d, %d)\n", irq, type, level);
#endif
	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
	    return (NULL);

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("intr_establish: bogus irq or type");

	iq = &isa_intrq[irq];

	switch (iq->iq_ist) {
	case IST_NONE:
		iq->iq_ist = type;
#if 0
		printf("Setting irq %d to type %d - ", irq, type);
#endif
		if (irq < 8) {
			outb(0x4d0, (inb(0x4d0) & ~(1 << irq))
			    | ((type == IST_LEVEL) ? (1 << irq) : 0));
/*			printf("%02x\n", inb(0x4d0));*/
		} else {
			outb(0x4d1, (inb(0x4d1) & ~(1 << irq))
			    | ((type == IST_LEVEL) ? (1 << irq) : 0));
/*			printf("%02x\n", inb(0x4d1));*/
		}
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (iq->iq_ist == type)
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    isa_intr_typename(iq->iq_ist),
			    isa_intr_typename(type));
		break;
	}

	ih->ih_func = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_ipl = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_irq,
	    &evcount_intr);

	/* do not stop us */
	oldirqstate = disable_interrupts(I32_bit);
	
	TAILQ_INSERT_TAIL(&iq->iq_list, ih, ih_list);

	intr_calculatemasks();
	restore_interrupts(oldirqstate);	
	
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
	struct intrq *iq = &isa_intrq[ih->ih_irq];
	int irq = ih->ih_irq;
	u_int oldirqstate;
	
	if (!LEGAL_IRQ(irq))
		panic("intr_disestablish: bogus irq");

	oldirqstate = disable_interrupts(I32_bit);

	TAILQ_REMOVE(&iq->iq_list, ih, ih_list);

	intr_calculatemasks();

	restore_interrupts(oldirqstate);

	evcount_detach(&ih->ih_count);
	free(ih, M_DEVBUF);

	if (TAILQ_EMPTY(&(iq->iq_list)))
		iq->iq_ist = IST_NONE;
}

/*
 * isa_intr_init()
 *
 * Initialise the ISA ICU and attach an ISA interrupt handler to the
 * ISA interrupt line on the footbridge.
 */
void
isa_intr_init(void)
{
	static void *isa_ih;
 	struct intrq *iq;
 	int i;
 
 	/* 
 	 * should get the parent here, but initialisation order being so
 	 * strange I need to check if it's available
 	 */
 	for (i = 0; i < ICU_LEN; i++) {
 		iq = &isa_intrq[i];
 		TAILQ_INIT(&iq->iq_list);
 	}
	
	isa_icu_init();
	intr_calculatemasks();
	/* something to break the build in an informative way */
#ifndef ISA_FOOTBRIDGE_IRQ 
#warning Before using isa with footbridge you must define ISA_FOOTBRIDGE_IRQ
#endif
	isa_ih = footbridge_intr_claim(ISA_FOOTBRIDGE_IRQ, IPL_BIO, NULL,
	    isa_irqdispatch, NULL);
	
}

/* Static array of ISA DMA segments. We only have one on CATS */
#if NISADMA > 0
struct arm32_dma_range machdep_isa_dma_ranges[1];
#endif

void
isa_footbridge_init(iobase, membase)
	u_int iobase, membase;
{
#if NISADMA > 0
	extern struct arm32_dma_range *footbridge_isa_dma_ranges;
	extern int footbridge_isa_dma_nranges;

	machdep_isa_dma_ranges[0].dr_sysbase = bootconfig.dram[0].address;
	machdep_isa_dma_ranges[0].dr_busbase = bootconfig.dram[0].address;
	machdep_isa_dma_ranges[0].dr_len = (16 * 1024 * 1024);

	footbridge_isa_dma_ranges = machdep_isa_dma_ranges;
	footbridge_isa_dma_nranges = 1;
#endif

	isa_io_init(iobase, membase);
}

void
isa_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{
	/*
	 * Since we can only have one ISA bus, we just use a single
	 * statically allocated ISA chipset structure.  Pass it up
	 * now.
	 */
	iba->iba_ic = &isa_chipset_tag;
#if NISADMA > 0
	isa_dma_init();
#endif
}

int
isa_irqdispatch(arg)
	void *arg;
{
	struct clockframe *frame = arg;
	int irq;
	struct intrq *iq;
	struct intrhand *ih;
	u_int iack;
	int res = 0;

	iack = *((u_int *)(DC21285_PCI_IACK_VBASE));
	iack &= 0xff;
	if (iack < 0x20 || iack > 0x2f) {
		printf("isa_irqdispatch: %x\n", iack);
		return(0);
	}

	irq = iack & 0x0f;
	iq = &isa_intrq[irq];
	for (ih = TAILQ_FIRST(&iq->iq_list); res != 1 && ih != NULL;
		     ih = TAILQ_NEXT(ih, ih_list)) {
		res = (*ih->ih_func)(ih->ih_arg ? ih->ih_arg : frame);
		if (res)
			ih->ih_count.ec_count++;
	}
	return res;
}


void
isa_fillw(val, addr, len)
	u_int val;
	void *addr;
	size_t len;
{
	if ((u_int)addr >= isa_mem_data_vaddr()
	    && (u_int)addr < isa_mem_data_vaddr() + 0x100000) {
		bus_size_t offset = ((u_int)addr) & 0xfffff;
		bus_space_set_region_2(&isa_mem_bs_tag,
		    (bus_space_handle_t)isa_mem_bs_tag.bs_cookie, offset,
		    val, len);
	} else {
		u_short *ptr = addr;

		while (len > 0) {
			*ptr++ = val;
			--len;
		}
	}
}
