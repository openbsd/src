/*	$OpenBSD: isabr.c,v 1.3 2004/01/30 22:38:30 miod Exp $	*/

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
#include <machine/powerpc.h>
#include <machine/intr.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#define	IRQ_SLAVE	2

struct isabr_softc {
	struct	device sc_dv;
	struct	ppc_isa_bus ppc_isa_cs;
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

void *isabr_intr_establish(void *, int, int, int, int (*)(void *), void *,
    char *);
void isabr_intr_disestablish (void *, void*);
void isabr_iointr(void);
void isabr_initicu (void);
static void intr_calculatemasks (void);
static int fakeintr(void *a);

/* These may be initialize early in _machdep.c for console configuration. */
struct ppc_bus_space	ppc_isa_io;
struct ppc_bus_space	ppc_isa_mem;

/* used by isa_inb() isa_outb() */
bus_space_handle_t ppc_isa_io_vaddr;


#define LEGAL_IRQ(x)    ((x) >= 0 && (x) < ICU_LEN && (x) != 2)

int	imen = 0xffffffff;
int	intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

const struct pci_matchid isabr_devices[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8231_ISA }
};

int
isabrmatch(struct device *parent, void *cfdata, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, isabr_devices,
	    sizeof(isabr_devices)/sizeof(isabr_devices[0])));
}


void isabr_collect_preconf_intr(void);
void
isabr_collect_preconf_intr()
{
	int i;
	for (i = 0; i < ppc_configed_intr_cnt; i++) {
		isabr_intr_establish(NULL, ppc_configed_intr[i].ih_irq,
		    IST_LEVEL, ppc_configed_intr[i].ih_level,
		    ppc_configed_intr[i].ih_fun, ppc_configed_intr[i].ih_arg,
		    ppc_configed_intr[i].ih_what);
	}
}


typedef void (void_f) (void);
extern void_f *pending_int_f;
void isa_do_pending_int(void);

void
isabrattach(struct device *parent, struct device *self, void *aux)
{
	struct isabr_softc *sc = (struct isabr_softc *)self;
	struct isabus_attach_args iba;
	struct pci_attach_args *pa = aux;
	extern intr_establish_t *intr_establish_func;
	extern intr_disestablish_t *intr_disestablish_func;


	pending_int_f = isa_do_pending_int;
	intr_establish_func = isabr_intr_establish;
	intr_disestablish_func = isabr_intr_disestablish;
	install_extint(isabr_iointr);

	printf("\n");

	/* Initialize interrupt controller */
	isabr_initicu();

	/* set up interrupt handlers */

	/*XXX we may remove the bushook part of the softc struct... */
	sc->sc_bus.bh_dv = (struct device *)sc;
	sc->sc_bus.bh_type = BUS_ISABR;

	sc->ppc_isa_cs.ic_intr_establish = isabr_intr_establish;
	sc->ppc_isa_cs.ic_intr_disestablish = isabr_intr_disestablish;

	iba.iba_busname = "isa";
	ppc_isa_io = *pa->pa_iot;
	ppc_isa_io.bus_base  = 0xfe000000;
	ppc_isa_mem.bus_base = 0xfd000000;
	ppc_isa_mem = *pa->pa_memt;
	iba.iba_iot = &ppc_isa_io;
	iba.iba_memt = &ppc_isa_mem;
	iba.iba_ic = &sc->ppc_isa_cs;

	/* XXX magic numbers, _vaddr used in isa_inb()/isa_outb() */
	if (bus_space_map (&ppc_isa_io, 0x0000, 0x2000, 0, &ppc_isa_io_vaddr))
		printf("unable to map io vaddr\n");

	isabr_collect_preconf_intr();

	config_found(self, &iba, isabrprint);
}

int
isabrprint(void *aux, const char *pnp)
{
	struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        return (UNCONF);
}


/*
 *	Interrupt system driver code
 *	============================
 */
static int
fakeintr(void *a)
{
	return 0;
}

void isa_setirqstat(int irq, int enabled, int type);

void
isa_setirqstat(int irq, int enabled, int type)
{
	intrtype[irq] = type;
	return;
}
/*
 * Recalculate the interrupt masks from scratch.
 * We could code special registry and deregistry versions of this function that
 * would be faster, but the code would be nastier, and we don't expect this to
 * happen very much anyway.
 */
static void
intr_calculatemasks()
{
	int irq, level, levels;
	struct intrhand *q;
	int irqs;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = IPL_NONE; level <= IPL_HIGH; level++) {
		irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		imask[level] = irqs | SINT_MASK;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_TTY] |= imask[IPL_NET];
	imask[IPL_IMP] |= imask[IPL_TTY];
	imask[IPL_CLOCK] |= imask[IPL_IMP] | SPL_CLOCK;

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0x00000000;
	imask[IPL_HIGH] = 0xffffffff;

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];

		intrmask[irq] = irqs | SINT_MASK;
	}

	/* Lastly, determine which IRQs are actually in use. */
	{
		irqs = 0;
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

void nameinterrupt(int replace, char *newstr);

/*
 *	Establish a ISA bus interrupt.
 */
void *  
isabr_intr_establish(void *arg_ic, int irq, int type, int level,
    int (*ih_fun) (void *), void *ih_arg, char *ih_what)
{
	static struct intrhand fakehand = {NULL, fakeintr};
	static int inthnd_installed = 0;
	struct intrhand **p, *q, *ih;
	extern int cold;

	if (!inthnd_installed) {
		install_extint(isabr_iointr);
		inthnd_installed++;
	}

	nameinterrupt(irq, ih_what);

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
isabr_intr_disestablish(void *ic, void *arg)
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
	int emsr;
static int processing;

	if (processing)
		return;

	emsr = ppc_intr_disable();
	processing = 1;

	pcpl = splhigh();		/* Turn off all */
	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	hwpend &= ((1L << ICU_LEN) - 1);
	ipending &= ~hwpend;
	imen &= ~hwpend;
	while (hwpend) {
		vector = ffs(hwpend) - 1;
		hwpend &= ~(1L << vector);
		ih = intrhand[vector];
		while (ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
	}
	if ((ipending & SINT_CLOCK) & ~pcpl) {
		ipending &= ~SINT_CLOCK;
		softclock();
	}
	if ((ipending & SINT_NET) & ~pcpl) {
		extern int netisr;
		int pisr = netisr;
		netisr = 0;
		ipending &= ~SINT_NET;
		softnet(pisr);
	}
	cpl = pcpl;	/* Don't use splx... we are here already! */

	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);

	processing = 0;
	 ppc_intr_enable(emsr);
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
isabr_iointr()
{
	struct intrhand *ih;
	int o_imen, r_imen;
	u_int8_t isa_vector;
	int pcpl;
	u_int32_t pci_iack(void); /* XXX */

	/* what about enabling external interrupt in here? */
	pcpl = splhigh() ;	/* Turn off all */

	isa_vector = pci_iack();

	isa_vector &= (ICU_LEN - 1);	/* XXX Better safe than sorry */

	intrcnt[isa_vector]++;

	o_imen = imen;
	r_imen = 1 << isa_vector;
	imen |= r_imen;
	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);

	if ((pcpl & r_imen) != 0) {
		ipending |= r_imen;	/* Masked! Mark this as pending */
	} else {
		ih = intrhand[isa_vector];
		if (ih == NULL)
			printf("isa: spurious interrupt %d\n", isa_vector);

		while (ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
		imen = o_imen;
	}
#ifdef NO_SPECIAL_MASK_MODE
	isa_outb(IO_ICU1, 0x20);
	isa_outb(IO_ICU2, 0x20);
#else
	if (isa_vector > 7) {
		isa_outb(IO_ICU2, 0x60 | (isa_vector & 0x07));
	}
	isa_outb(IO_ICU1, 0x60 | (isa_vector > 7 ? 2 : isa_vector));
#endif
	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);

	ppc_intr_enable(1);

	splx(pcpl);	/* Process pendings. */
}


/*
 * Initialize the Interrupt controller logic.
 */
void
isabr_initicu()
{
	int i;

	for (i= 0; i < ICU_LEN; i++) {
		switch (i) {
		case 0:
		case 1:
		case 2:
		case 8:
		case 13:
			intrtype[i] = IST_EDGE;
			break;
		default:
			intrtype[i] = IST_NONE;
		}
	}

	isa_outb(IO_ICU1, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU1+1, 0);			/* starting at this vector */
	isa_outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
	isa_outb(IO_ICU1+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
#ifndef NO_SPECIAL_MASK_MODE
	isa_outb(IO_ICU1, 0x68);		/* special mask mode  */
#endif
	isa_outb(IO_ICU1, 0x0a);		/* Read IRR by default. */

	isa_outb(IO_ICU2, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU2+1, 8);			/* starting at this vector */
	isa_outb(IO_ICU2+1, IRQ_SLAVE);
	isa_outb(IO_ICU2+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
#ifndef NO_SPECIAL_MASK_MODE
	isa_outb(IO_ICU2, 0x68);		/* special mask mode */
#endif
	isa_outb(IO_ICU2, 0x0a);		/* Read IRR by default */
}
