/*	$OpenBSD: isabus.c,v 1.7 1998/08/25 08:37:24 pefo Exp $	*/

/*-
 * Copyright (c) 1995 Per Fogelstrom
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)isa.c	7.2 (Berkeley) 5/12/91
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/autoconf.h>
#include <machine/intr.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <powerpc/pci/mpc106reg.h>

#define IO_ELCR1 0x04d0
#define IO_ELCR2 0x04d1

#define	IRQ_SLAVE	2

struct isabr_softc {
	struct	device sc_dv;
	struct	p4e_isa_bus p4e_isa_cs;
	struct	bushook sc_bus;
};

/* Definition of the driver for autoconfig. */
int	isabrmatch(struct device *, void *, void *);
void	isabrattach(struct device *, struct device *, void *);
int	isabrprint(void *, const char *);

struct cfattach isabr_ca = {
	sizeof(struct isabr_softc), isabrmatch, isabrattach
};
struct cfdriver isabr_cd = {
	NULL, "isabr", DV_DULL, NULL, 0
};

void	*isabr_intr_establish __P((isa_chipset_tag_t, int, int, int,
			int (*)(void *), void *, char *));
void	isabr_intr_disestablish __P((isa_chipset_tag_t, void*));
void	isabr_iointr __P((unsigned int, struct clockframe *));
void	isabr_initicu __P((void));
void	intr_calculatemasks __P((void));

struct p4e_bus_space	p4e_isa_io = {
	MPC106_V_ISA_IO_SPACE, 1 
};

struct p4e_bus_space	p4e_isa_mem = {
	MPC106_V_PCI_MEM_SPACE, 1
};

int
isabrmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, isabr_cd.cd_name) == 0)
		return (1);

	{
		struct pci_attach_args *pa = aux;

		if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_SIO)
			return (1);
	}
	return (0);
}

typedef void (void_f) (void);
extern void_f *pending_int_f;
void isa_do_pending_int();
struct evcnt evirq[ICU_LEN*2];

void
isabrattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct isabr_softc *sc = (struct isabr_softc *)self;
	struct isabus_attach_args iba;

	/* notyet -dsr
	ppc_intr_setup( isabr_intr_establish, isabr_intr_disestablish);
	*/

	pending_int_f = isa_do_pending_int;

	printf("\n");

	/* Initialize interrupt controller */
	isabr_initicu();

	/* set up interrupt handlers */

/*XXX we may remove the bushook part of the softc struct... */
	sc->sc_bus.bh_dv = (struct device *)sc;
	sc->sc_bus.bh_type = BUS_ISABR;

	sc->p4e_isa_cs.ic_intr_establish = isabr_intr_establish;
	sc->p4e_isa_cs.ic_intr_disestablish = isabr_intr_disestablish;

	{
		int i;
		for (i = 0; i < (ICU_LEN*2); i++) {
			evcnt_attach(self,"intr", &evirq[i]);
			/* put one in so they always print XXX */
			evirq[i].ev_count++;
		}
	}

	iba.iba_busname = "isa";
	iba.iba_iot = (bus_space_tag_t)&p4e_isa_io;
	iba.iba_memt = (bus_space_tag_t)&p4e_isa_mem;
	iba.iba_ic = &sc->p4e_isa_cs;
	config_found(self, &iba, isabrprint);
}

int
isabrprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        printf(" isa_io_base 0x%lx isa_mem_base 0x%lx",
		p4e_isa_io.bus_base, p4e_isa_mem.bus_base);
        return (UNCONF);
}


/*
 *	Interrupt system driver code
 *	============================
 */
#define LEGAL_IRQ(x)    ((x) >= 0 && (x) < ICU_LEN && (x) != 2)

int	imen = 0xffffffff;
int	intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

int fakeintr(void *a) {return 0;}

void
isa_setirqstat(int irq, int enabled, int type)
{
	u_int8_t elcr[2];
	int icu, bit;

	icu = irq / 8;
	bit = irq % 8;
	elcr[0] = isa_inb(IO_ELCR1);
	elcr[1] = isa_inb(IO_ELCR2);

	if (type == IST_LEVEL) {
		elcr[icu] |= 1 << bit;
	} else {
		elcr[icu] &= ~(1 << bit);
	}
	isa_outb(IO_ELCR1, elcr[0]);
	isa_outb(IO_ELCR2, elcr[1]);
	return;
}
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
		imask[level] = irqs | SINT_MASK;
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
		intrmask[irq] = irqs | SINT_MASK;
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
		isa_outb(IO_ICU1 + 1, imen);
		isa_outb(IO_ICU2 + 1, imen >> 8);
	}
}

/*
 *	Establish a ISA bus interrupt.
 */
void *   
isabr_intr_establish(ic, irq, type, level, ih_fun, ih_arg, ih_what)
        isa_chipset_tag_t ic;
        int irq;
        int type;
        int level;
        int (*ih_fun) __P((void *));
        void *ih_arg;
        char *ih_what;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {NULL, fakeintr};
	extern int cold;

static int inthnd_installed = 0;

	if(!inthnd_installed) {
		install_extint(isabr_iointr);
		inthnd_installed++;
	}

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
			panic("intr_establish: can't share %s with %s irq %d",
			    isa_intr_typename(intrtype[irq]),
			    isa_intr_typename(type), irq);
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

	isa_setirqstat(irq, 1, type);

	return (ih);
}

void                    
isabr_intr_disestablish(ic, arg)
        isa_chipset_tag_t ic;
        void *arg;      
{               
	/* Not yet */
}

void
isa_do_pending_int()
{
	struct intrhand *ih;
	int vector;
	int pcpl;
	int hwpend;
	int emsr, dmsr;
static int processing;

	if(processing)
		return;

	processing = 1;
	__asm__ volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	__asm__ volatile("mtmsr %0" :: "r"(dmsr));

	pcpl = splhigh();		/* Turn off all */
	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	hwpend &= ((1L << ICU_LEN) - 1);
	ipending &= ~hwpend;
	imen &= ~hwpend;
	while(hwpend) {
		evirq[ICU_LEN].ev_count++;
		vector = ffs(hwpend) - 1;
		hwpend &= ~(1L << vector);
		ih = intrhand[vector];
		evirq[ICU_LEN+vector].ev_count++;
		while(ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
		ipending &= ~(1L << vector);
	}
	if((ipending & SINT_CLOCK) & ~pcpl) {
		ipending &= ~SINT_CLOCK;
		softclock();
	}
	if((ipending & SINT_NET) & ~pcpl) {
		extern int netisr;
		int pisr = netisr;
		netisr = 0;
		ipending &= ~SINT_NET;
		softnet(pisr);
	}
	cpl = pcpl;	/* Don't use splx... we are here already! */

	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);

	__asm__ volatile("mtmsr %0" :: "r"(emsr));
	processing = 0;
}

/*
 *  Process an interrupt from the ISA bus.
 *  When we get here remember we have "delayed" ipl mask
 *  settings from the spl<foo>() calls. Yes it's faster
 *  to do it like this because SPL's are done so frequently
 *  and interrupts are likely to *NOT* happen most of the
 *  times the spl level is changed.
 */
void
isabr_iointr(mask, cf)
        unsigned mask;
        struct clockframe *cf;
{
	struct intrhand *ih;
	int o_imen, r_imen;
	u_int8_t isa_vector;
	int pcpl;

	/* what about enabling external interrupt in here? */
	pcpl = splhigh() ;	/* Turn off all */

	isa_vector = *(volatile u_int8_t *)0xbffffff0;
	isa_vector &= (ICU_LEN - 1);	/* XXX Better safe than sorry */
	evirq[0].ev_count++;

	o_imen = imen;
	r_imen = 1 << (isa_vector & (ICU_LEN - 1));
	imen |= r_imen;

	if((pcpl & r_imen) != 0) {
		ipending |= r_imen;	/* Masked! Mark this as pending */
		evirq[isa_vector].ev_count++;
	}
	else {
		ih = intrhand[isa_vector];
		evirq[isa_vector].ev_count++;
		if(ih == NULL)
			printf("isa: spurious interrupt %d\n", isa_vector);

		while(ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
		imen = o_imen;
	}
	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);
	isa_outb(IO_ICU1, 0x20);
	isa_outb(IO_ICU2, 0x20);

	splx(pcpl);	/* Process pendings. */
}


/* 
 * Initialize the Interrupt controller logic.
 */
void
isabr_initicu()
{  
	int i;
	int elcr = 0;
	for (i= 0; i < ICU_LEN; i++) {
		switch (i) {
		case 0:
		case 1:
		case 2:
		case 8:
		case 13:
			intrtype[i] = IST_EDGE;
			elcr |=  (1 << i);
			break;
		default:
			intrtype[i] = IST_NONE;
		}
	}

	isa_outb(IO_ELCR1, elcr);		/* always keep irq as edge */
	isa_outb(IO_ELCR2, elcr >> 8);		/* Clear level int mask 8-15 */

	isa_outb(IO_ICU1, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU1+1, 0);			/* starting at this vector */
	isa_outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
	isa_outb(IO_ICU1+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	isa_outb(IO_ICU1, 0x68);		/* special mask mode  */
	isa_outb(IO_ICU1, 0x0a);		/* Read IRR by default. */

	isa_outb(IO_ICU2, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU2+1, 8);			/* staring at this vector */
	isa_outb(IO_ICU2+1, IRQ_SLAVE);
	isa_outb(IO_ICU2+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	isa_outb(IO_ICU2, 0x68);		/* special mask mode */
	isa_outb(IO_ICU2, 0x0a);		/* Read IRR by default */
}	       

