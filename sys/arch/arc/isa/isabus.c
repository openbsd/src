/*	$OpenBSD: isabus.c,v 1.15 1998/03/16 09:38:46 pefo Exp $	*/
/*	$NetBSD: isa.c,v 1.33 1995/06/28 04:30:51 cgd Exp $	*/

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

#include <mips/archtype.h>
#include <arc/pica/pica.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <arc/isa/timerreg.h>
#include <arc/isa/spkrreg.h>
#include <arc/isa/isa_machdep.h>

static int beeping;

#define	IRQ_SLAVE	2
#define ICU_LEN		16

struct isabr_softc {
	struct	device sc_dv;
	struct	arc_isa_bus arc_isa_cs;
	struct	abus sc_bus;
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
int	isabr_iointr __P((unsigned int, struct clockframe *));
void	isabr_initicu __P((void));
void	intr_calculatemasks __P((void));
int	fakeintr __P((void *));

int
isabrmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

        /* Make sure that we're looking for a ISABR. */
        if (strcmp(ca->ca_name, isabr_cd.cd_name) != 0)
                return (0);

	return (1);
}

void
isabrattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct isabr_softc *sc = (struct isabr_softc *)self;
	struct isabus_attach_args iba;

	printf("\n");

	/* Initialize interrupt controller */
	isabr_initicu();

	/* set up interrupt handlers */
	switch(system_type) {
	case ACER_PICA_61:
	case MAGNUM:
		set_intr(INT_MASK_2, isabr_iointr, 3);
		break;
	case DESKSTATION_TYNE:
		set_intr(INT_MASK_2, isabr_iointr, 2);
		break;
	case DESKSTATION_RPC44:
		set_intr(INT_MASK_2, isabr_iointr, 2);
		break;
	default:
		panic("isabrattach: unkown system_type!");
	}

/*XXX we may remove the abus part of the softc struct... */
	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_ISABR;

	sc->arc_isa_cs.ic_intr_establish = isabr_intr_establish;
	sc->arc_isa_cs.ic_intr_disestablish = isabr_intr_disestablish;

	iba.iba_busname = "isa";
	iba.iba_iot = (bus_space_tag_t)&arc_bus_io;
	iba.iba_memt = (bus_space_tag_t)&arc_bus_mem;
	iba.iba_ic = &sc->arc_isa_cs;
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
		arc_bus_io.bus_base, arc_bus_mem.bus_base);
        return (UNCONF);
}


/*
 *	Interrupt system driver code
 *	============================
 */
#define LEGAL_IRQ(x)    ((x) >= 0 && (x) < ICU_LEN && (x) != 2)

int	imen;
int	intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];

int fakeintr(void *a) {return 0;}

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

void                    
isabr_intr_disestablish(ic, arg)
        isa_chipset_tag_t ic;
        void *arg;      
{               

}

/*
 *	Process an interrupt from the ISA bus.
 */
int
isabr_iointr(mask, cf)
        unsigned mask;
        struct clockframe *cf;
{
	struct intrhand *ih;
	int isa_vector;
	int o_imen;
	char vector;

	switch(system_type) {
	case ACER_PICA_61:
	case MAGNUM:
		isa_vector = in32(R4030_SYS_ISA_VECTOR) & (ICU_LEN - 1);
		break;

	case DESKSTATION_TYNE:
		isa_outb(IO_ICU1, 0x0f);	/* Poll */
		vector = isa_inb(IO_ICU1);
		if(vector > 0 || (isa_vector = vector & 7) == 2) { 
			isa_outb(IO_ICU2, 0x0f);
			vector = isa_inb(IO_ICU2);
			if(vector > 0) {
				printf("isa: spurious interrupt.\n");
				return(~0);
			}
			isa_vector = (vector & 7) | 8;
		}
		break;

	case DESKSTATION_RPC44:
		isa_outb(IO_ICU1, 0x0f);	/* Poll */
		vector = isa_inb(IO_ICU1);
		if(vector > 0 || (isa_vector = vector & 7) == 2) { 
			isa_outb(IO_ICU2, 0x0f);
			vector = isa_inb(IO_ICU2);
			if(vector > 0) {
				printf("isa: spurious interrupt.\n");
				return(~0);
			}
			isa_vector = (vector & 7) | 8;
		}
		break;
	}

	o_imen = imen;
	imen |= 1 << (isa_vector & (ICU_LEN - 1));
	if(isa_vector & 0x08) {
		isa_inb(IO_ICU2 + 1);
		isa_outb(IO_ICU2 + 1, imen >> 8);
		isa_outb(IO_ICU2, 0x60 + (isa_vector & 7));
		isa_outb(IO_ICU1, 0x60 + IRQ_SLAVE);
	}
	else {
		isa_inb(IO_ICU1 + 1);
		isa_outb(IO_ICU1 + 1, imen);
		isa_outb(IO_ICU1, 0x60 + isa_vector);
	}
	ih = intrhand[isa_vector];
	if(isa_vector == 0) {	/* Clock */	/*XXX*/
		(*ih->ih_fun)(cf);
		ih = ih->ih_next;
	}
	while(ih) {
		(*ih->ih_fun)(ih->ih_arg);
		ih = ih->ih_next;
	}
	imen = o_imen;
	isa_inb(IO_ICU1 + 1);
	isa_inb(IO_ICU2 + 1);
	isa_outb(IO_ICU1 + 1, imen);
	isa_outb(IO_ICU2 + 1, imen >> 8);

	return(~0);  /* Dont reenable */
}


/* 
 * Initialize the Interrupt controller logic.
 */
void
isabr_initicu()
{  

	isa_outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	isa_outb(IO_ICU1+1, 0);			/* starting at this vector index */
	isa_outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
	isa_outb(IO_ICU1+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	isa_outb(IO_ICU1, 0x68);		/* special mask mode (if available) */
	isa_outb(IO_ICU1, 0x0a);		/* Read IRR by default. */
#ifdef REORDER_IRQ  
	isa_outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */
#endif

	isa_outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	isa_outb(IO_ICU2+1, 8);			/* staring at this vector index */
	isa_outb(IO_ICU2+1, IRQ_SLAVE);
	isa_outb(IO_ICU2+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	isa_outb(IO_ICU2, 0x68);		/* special mask mode (if available) */
	isa_outb(IO_ICU2, 0x0a);		/* Read IRR by default. */
}	       


/*
 *	SPEAKER BEEPER...
 */
void
sysbeepstop(arg)
	void *arg;
{
	int s;

	/* disable counter 2 */
	s = splhigh();
	isa_outb(PITAUX_PORT, isa_inb(PITAUX_PORT) & ~PIT_SPKR);
	splx(s);
	beeping = 0;
}

void
sysbeep(pitch, period)
	int pitch, period;
{
	static int last_pitch, last_period;
	int s;
	extern int cold;

	if (cold)
		return;		/* Can't beep yet. */

	if (beeping)
		untimeout(sysbeepstop, 0);
	if (!beeping || last_pitch != pitch) {
		s = splhigh();
		isa_outb(TIMER_MODE, TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		isa_outb(TIMER_CNTR2, TIMER_DIV(pitch) % 256);
		isa_outb(TIMER_CNTR2, TIMER_DIV(pitch) / 256);
		isa_outb(PITAUX_PORT, isa_inb(PITAUX_PORT) | PIT_SPKR);
		splx(s);
	}
	last_pitch = pitch;
	beeping = last_period = period;
	timeout(sysbeepstop, 0, period);
}
