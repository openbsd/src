/*	$OpenBSD: isabus.c,v 1.1.1.1 1997/02/06 16:02:42 pefo Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <wgrisc/wgrisc/wgrisctype.h>
#include <wgrisc/riscbus/riscbus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <wgrisc/isa/isa_machdep.h>

static int beeping;

struct isabr_softc {
	struct	device sc_dv;
	struct	wgrisc_isa_bus wgrisc_isa_cs;
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

extern int cputype;

int
isabrmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
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

	/* set up interrupt handlers */
	switch(cputype) {
	case WGRISC9100:
		set_intr(INT_MASK_3 | INT_MASK_5, isabr_iointr, 1);
		break;
	default:
		panic("isabrattach: unkown cputype!");
	}

/*XXX we may remove the abus part of the softc struct... */
	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_ISABR;

	sc->wgrisc_isa_cs.ic_intr_establish = isabr_intr_establish;
	sc->wgrisc_isa_cs.ic_intr_disestablish = isabr_intr_disestablish;

	iba.iba_busname = "isa";
	iba.iba_iot = (bus_space_tag_t)isa_io_base;
	iba.iba_memt = (bus_space_tag_t)isa_mem_base;
	iba.iba_ic = &sc->wgrisc_isa_cs;
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
        printf(" isa_io_base 0x%lx isa_mem_base 0x%lx", isa_io_base, isa_mem_base);
        return (UNCONF);
}

/*
 *	Interrupt system driver code
 *	============================
 */

int	intmask;
int	intrmask[16], intrlevel[16];
struct intrhand *intrhand[16];

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
	for (irq = 0; irq < 16; irq++) {
		register int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < 5; level++) {
		register int irqs = 0;
		for (irq = 0; irq < 8; irq++)
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
	for (irq = 0; irq < 16; irq++) {
		register int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];
		intrmask[irq] = irqs | SIR_ALLMASK;
	}

	/* Lastly, determine which IRQs are actually in use. */
	{
		register int irqs = 0;
		for (irq = 0; irq < 16; irq++)
			if (intrhand[irq])
				irqs |= 1 << irq;
		intmask = ~irqs;
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

	switch(irq) {
	case 5:
		break;
	case 6:
		intmask |= R3715_ISAIRQ_6;
		break;
	case 7:
		intmask |= R3715_ISAIRQ_7;
		break;
	case 9:
		intmask |= R3715_ISAIRQ_9;
		break;
	case 10:
		intmask |= R3715_ISAIRQ_10;
		break;
	case 11:
		intmask |= R3715_ISAIRQ_11;
		break;
	default:
		printf("non available irq '%d' requested\n",irq);
		panic("isa_intr_establish: can't establish");
	}
	out32(R3715_INT_MASK, intmask);

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
	int cause;

	do {
		switch(cputype) {
		case WGRISC9100:
			cause = in32(R3715_INT_CAUSE) & in32(R3715_INT_MASK);
			if(mask & INT_MASK_5) {
				isa_vector = 5;
				mask = 0;	/* Do this only once */
			}
			else if(cause & R3715_ISAIRQ_11)
				isa_vector = 11;
			else if(cause & R3715_ISAIRQ_7)
				isa_vector = 7;
			else if(cause & R3715_ISAIRQ_6)
				isa_vector = 6;
			else if(cause & R3715_ISAIRQ_9)
				isa_vector = 9;
			else if(cause & R3715_ISAIRQ_10)
				isa_vector = 10;
			else
				isa_vector = -1;
			break;
		}

		ih = intrhand[isa_vector];
		while(ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}
	} while(isa_vector != -1);

	return(~0);  /* Dont reenable */
}

