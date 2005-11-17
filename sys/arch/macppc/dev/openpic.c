/*	$OpenBSD: openpic.c,v 1.33 2005/11/17 15:03:51 drahn Exp $	*/

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
 *	@(#)isa.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <uvm/uvm.h>
#include <ddb/db_var.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <machine/powerpc.h>
#include <macppc/dev/openpicreg.h>
#include <dev/ofw/openfirm.h>

#define ICU_LEN 128
#define LEGAL_IRQ(x) ((x >= 0) && (x < ICU_LEN))

int o_intrtype[ICU_LEN], o_intrmask[ICU_LEN], o_intrlevel[ICU_LEN];
struct intrhand *o_intrhand[ICU_LEN] = { 0 };
int o_hwirq[ICU_LEN], o_virq[ICU_LEN];
unsigned int imen_o = 0xffffffff;
int o_virq_max;

static int fakeintr(void *);
static char *intr_typename(int type);
static void intr_calculatemasks(void);
static __inline int cntlzw(int x);
static int mapirq(int irq);
int openpic_prog_button(void *arg);
void openpic_enable_irq_mask(int irq_mask);

#define HWIRQ_MAX 27
#define HWIRQ_MASK 0x0fffffff

static __inline u_int openpic_read(int);
static __inline void openpic_write(int, u_int);
void openpic_set_enable_irq(int, int);
void openpic_enable_irq(int);
void openpic_disable_irq(int);
void openpic_init(void);
void openpic_set_priority(int, int);
static __inline int openpic_read_irq(int);
static __inline void openpic_eoi(int);

struct openpic_softc {
	struct device sc_dev;
};

int	openpic_match(struct device *parent, void *cf, void *aux);
void	openpic_attach(struct device *, struct device *, void *);
void	openpic_do_pending_int(void);
void	openpic_collect_preconf_intr(void);
void	ext_intr_openpic(void);

struct cfattach openpic_ca = {
	sizeof(struct openpic_softc),
	openpic_match,
	openpic_attach
};

struct cfdriver openpic_cd = {
	NULL, "openpic", DV_DULL
};

int
openpic_match(struct device *parent, void *cf, void *aux)
{
	char type[40];
	int pirq;
	struct confargs *ca = aux;

	bzero (type, sizeof(type));

	if (OF_getprop(ca->ca_node, "interrupt-parent", &pirq, sizeof(pirq))
	    == sizeof(pirq))
		return 0; /* XXX */

	if (strcmp(ca->ca_name, "interrupt-controller") == 0 ||
	    strcmp(ca->ca_name, "mpic") == 0) {
		OF_getprop(ca->ca_node, "device_type", type, sizeof(type));
		if (strcmp(type, "open-pic") == 0)
			return 1;
	}
	return 0;
}

typedef void  (void_f) (void);
extern void_f *pending_int_f;

vaddr_t openpic_base;
void * openpic_intr_establish( void * lcv, int irq, int type, int level,
	int (*ih_fun)(void *), void *ih_arg, char *name);
void openpic_intr_disestablish( void *lcp, void *arg);
void openpic_collect_preconf_intr(void);

void
openpic_attach(struct device *parent, struct device  *self, void *aux)
{
	struct confargs *ca = aux;
	extern intr_establish_t *intr_establish_func;
	extern intr_disestablish_t *intr_disestablish_func;
	extern intr_establish_t *mac_intr_establish_func;
	extern intr_disestablish_t *mac_intr_disestablish_func;

	openpic_base = (vaddr_t) mapiodev (ca->ca_baseaddr +
			ca->ca_reg[0], 0x40000);

	printf(": version 0x%x", openpic_read(OPENPIC_VENDOR_ID));

	openpic_init();

	pending_int_f = openpic_do_pending_int;
	intr_establish_func  = openpic_intr_establish;
	intr_disestablish_func  = openpic_intr_disestablish;
	mac_intr_establish_func  = openpic_intr_establish;
	mac_intr_disestablish_func  = openpic_intr_disestablish;
	install_extint(ext_intr_openpic);

#if 1
	openpic_collect_preconf_intr();
#endif

#if 1
	mac_intr_establish(parent, 0x37, IST_LEVEL,
		IPL_HIGH, openpic_prog_button, (void *)0x37, "prog button");
#endif
	ppc_intr_enable(1);

	printf("\n");
}

void
openpic_collect_preconf_intr()
{
	int i;
	for (i = 0; i < ppc_configed_intr_cnt; i++) {
#ifdef DEBUG
		printf("\n\t%s irq %d level %d fun %x arg %x",
		    ppc_configed_intr[i].ih_what, ppc_configed_intr[i].ih_irq,
		    ppc_configed_intr[i].ih_level, ppc_configed_intr[i].ih_fun,
		    ppc_configed_intr[i].ih_arg);
#endif
		openpic_intr_establish(NULL, ppc_configed_intr[i].ih_irq,
		    IST_LEVEL, ppc_configed_intr[i].ih_level,
		    ppc_configed_intr[i].ih_fun, ppc_configed_intr[i].ih_arg,
		    ppc_configed_intr[i].ih_what);
	}
}

static int
fakeintr(void *arg)
{

	return 0;
}

/*
 * Register an interrupt handler.
 */
void *
openpic_intr_establish(void *lcv, int irq, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, char *name)
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand;

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

	switch (o_intrtype[irq]) {
	case IST_NONE:
		o_intrtype[irq] = type;
		break;
	case IST_EDGE:
	case IST_LEVEL:
		if (type == o_intrtype[irq])
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    intr_typename(o_intrtype[irq]),
			    intr_typename(type));
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &o_intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
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
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, name, (void *)&o_hwirq[irq],
	    &evcount_intr);
	*p = ih;

	return (ih);
}

/*
 * Deregister an interrupt handler.
 */
void
openpic_intr_disestablish(void *lcp, void *arg)
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
	for (p = &o_intrhand[irq]; (q = *p) != NULL && q != ih; p = &q->ih_next)
		;
	if (q)
		*p = q->ih_next;
	else
		panic("intr_disestablish: handler not registered");

	evcount_detach(&ih->ih_count);
	free((void *)ih, M_DEVBUF);

	intr_calculatemasks();

	if (o_intrhand[irq] == NULL)
		o_intrtype[irq] = IST_NONE;
}


static char *
intr_typename(int type)
{

	switch (type) {
	case IST_NONE:
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
		for (q = o_intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		o_intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = IPL_NONE; level < IPL_NUM; level++) {
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++)
			if (o_intrlevel[irq] & (1 << level))
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
		register int irqs = 1 << irq;
		for (q = o_intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];
		o_intrmask[irq] = irqs | SINT_MASK;
	}

	/* Lastly, determine which IRQs are actually in use. */
	{
		register int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++) {
			if (o_intrhand[irq]) {
				irqs |= 1 << irq;
				openpic_enable_irq(o_hwirq[irq]);
			} else {
				openpic_disable_irq(o_hwirq[irq]);
			}
		}
		imen_o = ~irqs;
	}
}

/*
 * Map 64 irqs into 32 (bits).
 */
static int
mapirq(int irq)
{
	int v;

	/* irq in table already? */
	if (o_virq[irq] != 0)
		return o_virq[irq];

	if (irq < 0 || irq >= ICU_LEN)
		panic("invalid irq %d", irq);

	o_virq_max++;
	v = o_virq_max;
	if (v > HWIRQ_MAX)
		panic("virq overflow");

	o_hwirq[v] = irq;
	o_virq[irq] = v;
#if 0
printf("\nmapirq %x to %x\n", irq, v);
#endif

	return v;
}

/*
 * Count leading zeros.
 */
static __inline int
cntlzw(int x)
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
	int s;
	static int processing;

	if (processing)
		return;

	processing = 1;
	pcpl = splhigh();		/* Turn off all */
	s = ppc_intr_disable();

	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	imen_o &= ~hwpend;
	openpic_enable_irq_mask(~imen_o);
	hwpend &= HWIRQ_MASK;
	while (hwpend) {
		irq = 31 - cntlzw(hwpend);
		hwpend &= ~(1L << irq);
		ih = o_intrhand[irq];
		while(ih) {
			ppc_intr_enable(1);

			if ((*ih->ih_fun)(ih->ih_arg))
				ih->ih_count.ec_count++;

			(void)ppc_intr_disable();
			
			ih = ih->ih_next;
		}
	}

	/*out32rb(INT_ENABLE_REG, ~imen_o);*/

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
		if((ipending & SINT_TTY) & ~pcpl) {
			ipending &= ~SINT_TTY;
			softtty();
		}
	} while (ipending & (SINT_NET|SINT_CLOCK|SINT_TTY) & ~cpl);
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */
	ppc_intr_enable(s);
	processing = 0;
}

u_int
openpic_read(int reg)
{
	char *addr = (void *)(openpic_base + reg);

	return in32rb(addr);
}

void
openpic_write(int reg, u_int val)
{
	char *addr = (void *)(openpic_base + reg);

	out32rb(addr, val);
}

void
openpic_enable_irq_mask(int irq_mask)
{
	int irq;
	for ( irq = 0; irq <= o_virq_max; irq++) {
		if (irq_mask & (1 << irq))
			openpic_enable_irq(o_hwirq[irq]);
		else
			openpic_disable_irq(o_hwirq[irq]);
	}
}

void
openpic_set_enable_irq(int irq, int type)
{
	u_int x;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x &= ~(OPENPIC_IMASK|OPENPIC_SENSE_LEVEL|OPENPIC_SENSE_EDGE);
	if (type == IST_LEVEL)
		x |= OPENPIC_SENSE_LEVEL;
	else
		x |= OPENPIC_SENSE_EDGE;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}
void
openpic_enable_irq(int irq)
{
	u_int x;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x &= ~(OPENPIC_IMASK|OPENPIC_SENSE_LEVEL|OPENPIC_SENSE_EDGE);
	if (o_intrtype[o_virq[irq]] == IST_LEVEL)
		x |= OPENPIC_SENSE_LEVEL;
	else
		x |= OPENPIC_SENSE_EDGE;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_disable_irq(int irq)
{
	u_int x;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x |= OPENPIC_IMASK;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_set_priority(int cpu, int pri)
{
	u_int x;

	x = openpic_read(OPENPIC_CPU_PRIORITY(cpu));
	x &= ~OPENPIC_CPU_PRIORITY_MASK;
	x |= pri;
	openpic_write(OPENPIC_CPU_PRIORITY(cpu), x);
}

int
openpic_read_irq(int cpu)
{
	return openpic_read(OPENPIC_IACK(cpu)) & OPENPIC_VECTOR_MASK;
}

void
openpic_eoi(int cpu)
{
	openpic_write(OPENPIC_EOI(cpu), 0);
	openpic_read(OPENPIC_EOI(cpu));
}

void
ext_intr_openpic()
{
	int irq, realirq;
	int r_imen;
	int pcpl, ocpl;
	struct intrhand *ih;

	pcpl = cpl;

	realirq = openpic_read_irq(0);

	while (realirq != 255) {
		irq = o_virq[realirq];

		/* XXX check range */

		r_imen = 1 << irq;

		if ((pcpl & r_imen) != 0) {
			ipending |= r_imen;	/* Masked! Mark this as pending */
			openpic_disable_irq(realirq);
			openpic_eoi(0);
		} else {
			openpic_disable_irq(realirq);
			openpic_eoi(0);
			ocpl = splraise(o_intrmask[irq]);

			ih = o_intrhand[irq];
			while (ih) {
				ppc_intr_enable(1);

				if ((*ih->ih_fun)(ih->ih_arg))
					ih->ih_count.ec_count++;

				(void)ppc_intr_disable();
				ih = ih->ih_next;
			}

			uvmexp.intrs++;
			__asm__ volatile("":::"memory"); /* don't reorder.... */
			cpl = ocpl;
			__asm__ volatile("":::"memory"); /* don't reorder.... */
			openpic_enable_irq(realirq);
		}

		realirq = openpic_read_irq(0);
	}
	ppc_intr_enable(1);

	splx(pcpl);	/* Process pendings. */
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

/*
 * programmer_button function to fix args to Debugger.
 * deal with any enables/disables, if necessary.
 */
int
openpic_prog_button (void *arg)
{
#ifdef DDB
	if (db_console)
		Debugger();
#else
	printf("programmer button pressed, debugger not available\n");
#endif
	return 1;
}
