/*	$OpenBSD: openpic.c,v 1.2 2000/03/31 04:20:20 rahnds Exp $	*/

/*-
 * Copyright (c) 1995 Per Fogelstrom
 * Copyright (c) 1993, 1994 Charles M. Hannum.
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <powerpc/mac/openpicreg.h>

#define ICU_LEN 64
#define LEGAL_IRQ(x) ((x >= 0) && (x < ICU_LEN))

static int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
static struct intrhand *intrhand[ICU_LEN] = { 0 };
static int hwirq[ICU_LEN], virq[64];
unsigned int imen /* = 0xffffffff */; /* XXX */
static int virq_max = 0;

struct evcnt evirq[ICU_LEN];

static int fakeintr __P((void *));
static char *intr_typename(int type);
static void intr_calculatemasks();
static __inline int cntlzw(int x);
static int mapirq(int irq);
static int read_irq();
void openpic_enable_irq_mask(int irq_mask);

extern u_int32_t *heathrow_FCR;

#define HWIRQ_MAX 27
#define HWIRQ_MASK 0x0fffffff


static __inline u_int openpic_read __P((int));
static __inline void openpic_write __P((int, u_int));
void openpic_enable_irq __P((int));
void openpic_disable_irq __P((int));
void openpic_init();
void openpic_set_priority __P((int, int));
static __inline int openpic_read_irq __P((int));
static __inline void openpic_eoi __P((int));

struct openpic_softc {
	struct device sc_dev;
};

int	openpic_match __P((struct device *parent, void *cf, void *aux));
void	openpic_attach __P((struct device *, struct device *, void *));
void	openpic_do_pending_int();
void ext_intr_openpic();

struct cfattach openpic_ca = { 
	sizeof(struct openpic_softc),
	openpic_match,
	openpic_attach
};

struct cfdriver openpic_cd = {
	NULL, "openpic", DV_DULL
};

int
openpic_match(parent, cf, aux) 
	struct device *parent;
	void *cf;
	void *aux;
{
	char type[40];
	struct confargs *ca = aux;

	bzero (type, sizeof(type));

	if (strcmp(ca->ca_name, "interrupt-controller") == 0 ) {
		OF_getprop(ca->ca_node, "device_type", type, sizeof(type));
		if (strcmp(type,  "open-pic") == 0) {
			return 1;
		}
	}
	return 0;
}

u_int8_t *interrupt_reg;
typedef void  (void_f) (void);
extern void_f *pending_int_f;
static int prog_switch (void *arg);
typedef int mac_intr_handle_t;

typedef void     *(intr_establish_t) __P((void *, mac_intr_handle_t,
            int, int, int (*func)(void *), void *, char *));
typedef void     (intr_disestablish_t) __P((void *, void *));

vaddr_t openpic_base;
void * openpic_intr_establish( void * lcv, int irq, int type, int level,
	int (*ih_fun) __P((void *)), void *ih_arg, char *name);
void openpic_intr_disestablish( void *lcp, void *arg);

void
openpic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct openpic_softc *sc = (void *)self;
	extern intr_establish_t *intr_establish_func;
	extern intr_disestablish_t *intr_disestablish_func;
	extern intr_establish_t *mac_intr_establish_func;
	extern intr_disestablish_t *mac_intr_disestablish_func;

	openpic_base = (vaddr_t) mapiodev (ca->ca_baseaddr +
			ca->ca_reg[0], 0x22000);

	printf("version %x", openpic_read(OPENPIC_VENDOR_ID));

	openpic_init();

	pending_int_f = openpic_do_pending_int;
	intr_establish_func  = openpic_intr_establish;
	intr_disestablish_func  = openpic_intr_disestablish;
	mac_intr_establish_func  = openpic_intr_establish;
	mac_intr_disestablish_func  = openpic_intr_disestablish;
	install_extint(ext_intr_openpic);

#if 1
	mac_intr_establish(parent, 0x37, IST_LEVEL,
		IPL_HIGH, prog_switch, 0x37, "prog button");
#endif

	printf("\n");
}

static int
prog_switch (void *arg)
{
#ifdef DDB
        Debugger();
#else
	printf("programmer button pressed, debugger not available\n");
#endif
	return 1;
}

static int
fakeintr(arg)
	void *arg;
{

	return 0;
}

/*
 * Register an interrupt handler.
 */
void *
openpic_intr_establish(lcv, irq, type, level, ih_fun, ih_arg, name)
	void * lcv;
	int irq;
	int type;
	int level;
	int (*ih_fun) __P((void *));
	void *ih_arg;
	char *name;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand;
	extern int cold;

	fakehand.ih_next = NULL;
	fakehand.ih_fun  = fakeintr;

#if 0
printf("mac_intr_establish, hI %d L %d ", irq, type);
#endif

	irq = mapirq(irq);
#if 0
printf("vI %d ", irq);
#endif

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("intr_establish: bogus irq or type");

	switch (intrtype[irq]) {
	case IST_NONE:
		intrtype[irq] = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intrtype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    intr_typename(intrtype[irq]),
			    intr_typename(type));
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
	 * this with interrupts enabled and DON'T WANt the real routine called
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
	*p = ih;

	return (ih);
}

/*
 * Deregister an interrupt handler.
 */
void
openpic_intr_disestablish(lcp, arg)
	void *lcp;
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
	free((void *)ih, M_DEVBUF);

	intr_calculatemasks();

	if (intrhand[irq] == NULL)
		intrtype[irq] = IST_NONE;
}


static char *
intr_typename(type)
	int type;
{

	switch (type) {
        case IST_NONE :
		return ("none");
        case IST_PULSE:
		return ("pulsed");
        case IST_EDGE:
		return ("edge-triggered");
        case IST_LEVEL:
		return ("level-triggered");
	default:
		panic("intr_typename: invalid type %d", type);
#if 1 /* XXX */
		return ("unknown");
#endif
	}
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
		for (irq = 0; irq < ICU_LEN; irq++) {
			if (intrhand[irq]) {
				irqs |= 1 << irq;
				openpic_enable_irq(hwirq[irq]);
			} else {
				openpic_disable_irq(hwirq[irq]);
			}
		}
		imen = ~irqs;
	}
}
/*
 * Map 64 irqs into 32 (bits).
 */
static int
mapirq(irq)
	int irq;
{
	int v;

	if (irq < 0 || irq >= 64)
		panic("invalid irq");
	virq_max++;
	v = virq_max;
	if (v > HWIRQ_MAX)
		panic("virq overflow");

	hwirq[v] = irq;
	virq[irq] = v;
#if 0
printf("\nmapirq %x to %x\n", irq, v);
#endif

	return v;
}

/*
 * Count leading zeros.
 */
static __inline int
cntlzw(x)
	int x;
{
	int a;

	__asm __volatile ("cntlzw %0,%1" : "=r"(a) : "r"(x));

	return a;
}


void
openpic_do_pending_int()
{
	struct intrhand *ih;
	int irq;
	int pcpl;
	int hwpend;
	int emsr, dmsr;
	static int processing;

	if (processing)
		return;

	processing = 1;
	pcpl = splhigh();		/* Turn off all */
	asm volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	asm volatile("mtmsr %0" :: "r"(dmsr));

	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	imen &= ~hwpend;
	openpic_enable_irq_mask(~imen);
	hwpend &= HWIRQ_MASK;
	while (hwpend) {
		irq = 31 - cntlzw(hwpend);
		hwpend &= ~(1L << irq);
		ih = intrhand[irq];
		while(ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}

		evirq[hwirq[irq]].ev_count++;
	}

	/*out32rb(INT_ENABLE_REG, ~imen);*/

	do {
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
	} while (ipending & (SINT_NET|SINT_CLOCK) & ~cpl);
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */
	asm volatile("mtmsr %0" :: "r"(emsr));
	processing = 0;
}

u_int
openpic_read(reg)
	int reg;
{
	char *addr = (void *)(openpic_base + reg);

	return in32rb(addr);
}

void
openpic_write(reg, val)
	int reg;
	u_int val;
{
	char *addr = (void *)(openpic_base + reg);

	out32rb(addr, val);
}

void
openpic_enable_irq_mask(irq_mask)
int irq_mask;
{
	int irq;
	for ( irq = 0; irq <= virq_max; irq++) {
		if (irq_mask & (1 << irq)) {
			openpic_enable_irq(hwirq[irq]);
		} else {
			openpic_disable_irq(hwirq[irq]);
		}
	}
}
void
openpic_enable_irq(irq)
	int irq;
{
	u_int x;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x &= ~OPENPIC_IMASK;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_disable_irq(irq)
	int irq;
{
	u_int x;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x |= OPENPIC_IMASK;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_set_priority(cpu, pri)
        int cpu, pri;
{
	u_int x;

	x = openpic_read(OPENPIC_CPU_PRIORITY(cpu));
	x &= ~OPENPIC_CPU_PRIORITY_MASK;
	x |= pri;
	openpic_write(OPENPIC_CPU_PRIORITY(cpu), x);
}

int
openpic_read_irq(cpu)
	int cpu;
{
	return openpic_read(OPENPIC_IACK(cpu)) & OPENPIC_VECTOR_MASK;
}

void
openpic_eoi(cpu)
        int cpu;
{
        openpic_write(OPENPIC_EOI(cpu), 0);
        openpic_read(OPENPIC_EOI(cpu));
}

void
ext_intr_openpic()
{
	int irq, realirq;
	int r_imen;
	int pcpl;
	struct intrhand *ih;

	pcpl = splhigh();       /* Turn off all */

	realirq = openpic_read_irq(0);

	while (realirq != 255) {
		irq = virq[realirq];
		intrcnt[realirq]++;

		/* XXX check range */

		r_imen = 1 << irq;

		if ((pcpl & r_imen) != 0) {
			ipending |= r_imen;     /* Masked! Mark this as pending */
			openpic_disable_irq(realirq);
		} else {
			ih = intrhand[irq];
			while (ih) {
				(*ih->ih_fun)(ih->ih_arg);
				ih = ih->ih_next;
			}

#ifdef UVM
			uvmexp.intrs++;
#else
#endif
			evirq[realirq].ev_count++;
		}

		openpic_eoi(0);

		realirq = openpic_read_irq(0);
	}

	splx(pcpl);     /* Process pendings. */
}
void
openpic_init()
{
        int irq;
        u_int x;

        /* disable all interrupts */
        for (irq = 0; irq < 255; irq++)
                openpic_write(OPENPIC_SRC_VECTOR(irq), OPENPIC_IMASK);
        openpic_set_priority(0, 15);

        /* we don't need 8259 pass through mode */
        x = openpic_read(OPENPIC_CONFIG);
        x |= OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
        openpic_write(OPENPIC_CONFIG, x);

        /* send all interrupts to cpu 0 */
        for (irq = 0; irq < ICU_LEN; irq++)
                openpic_write(OPENPIC_IDEST(irq), 1 << 0);
        for (irq = 0; irq < ICU_LEN; irq++) {
                x = irq;
                x |= OPENPIC_IMASK;
                x |= OPENPIC_POLARITY_POSITIVE;
                x |= OPENPIC_SENSE_LEVEL;
                x |= 8 << OPENPIC_PRIORITY_SHIFT;
                openpic_write(OPENPIC_SRC_VECTOR(irq), x);
        }

        /* XXX set spurious intr vector */

        openpic_set_priority(0, 0);

        /* clear all pending interrunts */
        for (irq = 0; irq < ICU_LEN; irq++) {
                openpic_read_irq(0);
                openpic_eoi(0);
        }

        for (irq = 0; irq < ICU_LEN; irq++)
                openpic_disable_irq(irq);

        install_extint(ext_intr_openpic);
}
