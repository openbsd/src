/*	$OpenBSD: openpic.c,v 1.15 2004/05/14 18:29:39 miod Exp $	*/

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

#include <uvm/uvm_extern.h>

#include <ddb/db_var.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>

#include <mvmeppc/dev/openpicreg.h>
#include <mvmeppc/dev/ravenvar.h>
#include <mvmeppc/dev/ravenreg.h>

#define ICU_LEN		32
#define LEGAL_IRQ(x)	((x >= 0) && (x < ICU_LEN))
#define IO_ICU1		(isaspace_va + 0x20)
#define IO_ICU2		(isaspace_va + 0xa0)
#define IO_ELCR1	(isaspace_va + 0x4d0)
#define IO_ELCR2	(isaspace_va + 0x4d1)
#define IRQ_SLAVE	2
#define ICU_OFFSET	0
#define PIC_OFFSET	16

#define	PIC_SPURIOUS	0xff

unsigned char icu1_val = 0xff;
unsigned char icu2_val = 0xff;
unsigned char elcr1_val = 0x00;
unsigned char elcr2_val = 0x00;

int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
struct intrhand *intrhand[ICU_LEN];
int hwirq[ICU_LEN], virq[ICU_LEN];
unsigned int imen = 0xffffffff;
int virq_max;

struct evcnt evirq[ICU_LEN];

int fakeintr(void *);
const char *intr_typename(int type);
void intr_calculatemasks(void);
static __inline int cntlzw(int x);
int mapirq(int irq);
void openpic_enable_irq_mask(int irq_mask);

#define HWIRQ_MAX 27
#define HWIRQ_MASK 0x0fffffff

static __inline u_int openpic_read(int);
static __inline void openpic_write(int, u_int);
void openpic_enable_irq(int, int);
void openpic_disable_irq(int);
void openpic_init(void);
void openpic_set_priority(int, int);
static __inline int openpic_iack(int);
static __inline void openpic_eoi(int);
void openpic_initirq(int, int, int);

void i8259_init(void);
int i8259_intr(void);
void i8259_enable_irq(int, int);
void i8259_disable_irq(int);
static __inline void i8259_eoi(int);
void *i8259_intr_establish(void *, int, int, int, int (*)(void *), void *,
    char *);
void i8259_set_irq_mask(void);

struct openpic_softc {
	struct device sc_dev;
};

int openpic_match(struct device *parent, void *cf, void *aux);
void openpic_attach(struct device *, struct device *, void *);
void openpic_do_pending_int(void);
void ext_intr_openpic(void);

struct cfattach openpic_ca = {
	sizeof(struct openpic_softc), openpic_match, openpic_attach
};

struct cfdriver openpic_cd = {
	NULL, "openpic", DV_DULL
};

/*
 * ISA IRQ for PCI IRQ to MPIC IRQ routing.
 * From MVME2600APG tables 5.2 and 5.3
 */
const struct pci_route {
	int pci;
	int openpic;
} pci_routes[] = {
	{ 10, 2 },
	{ 11, 5 },
	{ 14, 3 },
	{ 15, 4 },
	{ 0, 0 }
};

int
openpic_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	/* We must be a child of the raven device */
	if (strcmp(parent->dv_cfdata->cf_driver->cd_name, "raven") != 0)
		return (0);
	/* If there is a raven, then there is a mpic! */
	return 1;
}

u_int8_t *interrupt_reg;
typedef void (void_f) (void);
extern void_f *pending_int_f;
int abort_switch (void *arg);
int i8259_dummy(void *arg);

typedef int mac_intr_handle_t;

typedef void *(intr_establish_t)(void *, int, int, int, int (*)(void *),
    void *, char *);
typedef void (intr_disestablish_t)(void *, void *);

vaddr_t openpic_base;
extern vaddr_t isaspace_va;

void * openpic_intr_establish(void *, int, int, int, int (*)(void *), void *,
    char *);
void openpic_intr_disestablish(void *, void *);
void openpic_collect_preconf_intr(void);

void
openpic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	extern intr_establish_t *intr_establish_func;
	extern intr_disestablish_t *intr_disestablish_func;

	if ((openpic_base = (vaddr_t)mapiodev(MPCIC_BASE, MPCIC_SIZE)) == NULL) {
		printf(": can't map MPCIC!\n");
		return;
	}

	/* the ICU area in isa space already mapped */

	printf(": version 0x%x", openpic_read(OPENPIC_FEATURE) & 0xFF);

	i8259_init();
	openpic_init();

	pending_int_f = openpic_do_pending_int;
	intr_establish_func = i8259_intr_establish;
	intr_disestablish_func = openpic_intr_disestablish;

	openpic_collect_preconf_intr();

	/*
	 * i8259 interrupts are chained to openpic interrupt #0
	 */
	openpic_intr_establish(parent, 0x00, IST_LEVEL, IPL_HIGH,
	    i8259_dummy, NULL, "8259 Interrupt");

	i8259_intr_establish(parent, 0x08, IST_EDGE, IPL_HIGH,
	    abort_switch, NULL, "abort button");

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

int
abort_switch(void *arg)
{
#ifdef DDB
	if (db_console)
		Debugger();
#else
	printf("Abort button pressed, debugger not available.\n");
#endif
	return 1;
}

int
i8259_dummy(void *arg)
{
	/* All the 8259 handling happens in ext_intr_openpic(), actually. */
	return 1;
}

int
fakeintr(arg)
	void *arg;
{
	return 0;
}

/*
 * Register an ISA interrupt handler.
 */
void *
i8259_intr_establish(lcv, irq, type, level, ih_fun, ih_arg, what)
	void * lcv;
	int irq;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
	char *what;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand;

	fakehand.ih_next = NULL;
	fakehand.ih_fun = fakeintr;

#if 0
	printf("i8259_intr_establish, %d, %s", irq, (type == IST_EDGE) ? "EDGE":"LEVEL"));
#endif
	irq = mapirq(irq + ICU_OFFSET);

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("i8259_intr_establish: can't malloc handler info");

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("i8259_intr_establish: bogus irq or type");

	switch (intrtype[irq]) {
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
	ih->ih_what = what;
	*p = ih;

	return (ih);
}


/*
 * Register a PCI interrupt handler.
 */
void *
openpic_intr_establish(lcv, irq, type, level, ih_fun, ih_arg, what)
	void * lcv;
	int irq;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
	char *what;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand;
	const struct pci_route *pr;

	fakehand.ih_next = NULL;
	fakehand.ih_fun = fakeintr;

	for (pr = pci_routes; pr->pci != 0; pr++)
		if (pr->pci == irq) {
			irq = pr->openpic;
			break;
		}

	irq = mapirq(irq + PIC_OFFSET);

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");

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
	ih->ih_what = what;
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

const char *
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
#ifdef DIAGNOSTIC
	default:
		panic("intr_typename: invalid type %d", type);
#endif
	}
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
	for (level = IPL_NONE; level < IPL_NUM; level++) {
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
	irqs = 0;
	for (irq = 0; irq < ICU_LEN; irq++) {
		if (intrhand[irq]) {
			irqs |= 1 << irq;

			if (hwirq[irq] >= PIC_OFFSET)
				openpic_enable_irq(hwirq[irq], intrtype[irq]);
			else
				i8259_enable_irq(hwirq[irq], intrtype[irq]);
		} else {
			if (hwirq[irq] >= PIC_OFFSET)
				openpic_disable_irq(hwirq[irq]);
			else
				i8259_disable_irq(hwirq[irq]);
		}
	}

	/* always enable the chained 8259 interrupt */
	i8259_enable_irq(IRQ_SLAVE, IST_EDGE);

	imen = ~irqs;
	i8259_set_irq_mask();
}

/*
 * Map 64 irqs into 32 (bits).
 */
int
mapirq(irq)
	int irq;
{
	int v;

#ifdef DIAGNOSTIC
	if (irq < 0 || irq >= ICU_LEN)
		panic("invalid irq");
#endif

	virq_max++;
	v = virq_max;
	if (v > HWIRQ_MAX)
		panic("virq overflow");

	hwirq[v] = irq;
	virq[irq] = v;
#ifdef DEBUG
	printf("mapirq %x to %x\n", irq, v);
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
	int s;
	static int processing;

	if (processing)
		return;

	processing = 1;
	pcpl = splhigh();		/* Turn off all */
	s = ppc_intr_disable();

	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	imen &= ~hwpend;
	openpic_enable_irq_mask(~imen);
	hwpend &= HWIRQ_MASK;
	while (hwpend) {
		irq = 31 - cntlzw(hwpend);
		hwpend &= ~(1L << irq);
		ih = intrhand[irq];
		while (ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}

		evirq[hwirq[irq]].ev_count++;
	}

	do {
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
#if 0
		if ((ipending & SINT_TTY) & ~pcpl) {
			ipending &= ~SINT_TTY;
			softtty();
		}
#endif
	} while (ipending & (SINT_NET|SINT_CLOCK/*|SINT_TTY*/) & ~cpl);
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */

#if 0
	i8259_set_irq_mask();
#endif

	ppc_intr_enable(s);
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

	for (irq = 0; irq <= virq_max; irq++)
		if (irq_mask & (1 << irq)) {
			if (hwirq[irq] >= PIC_OFFSET)
				openpic_enable_irq(hwirq[irq], intrtype[irq]);
			else
				i8259_enable_irq(hwirq[irq], intrtype[irq]);
		} else {
			if (hwirq[irq] >= PIC_OFFSET)
				openpic_disable_irq(hwirq[irq]);
			else
				i8259_disable_irq(hwirq[irq]);
		}

	i8259_set_irq_mask();
}

void
openpic_enable_irq(irq, type)
	int irq;
	int type;
{
	u_int x;

	/* skip invalid irqs */
	if (irq < 0)
		return;
	if (irq >= PIC_OFFSET)
		irq -= PIC_OFFSET;

	while ((x = openpic_read(OPENPIC_SRC_VECTOR(irq))) & OPENPIC_ACTIVITY) {
		x = openpic_iack(0);
		openpic_eoi(0);
	}

	x &= ~(OPENPIC_IMASK|OPENPIC_SENSE_LEVEL|OPENPIC_SENSE_EDGE|
			 OPENPIC_POLARITY_POSITIVE);
#if 1
	if (irq == 0) {
		x |= OPENPIC_POLARITY_POSITIVE;
	}
#endif
	if (type == IST_LEVEL)
		x |= OPENPIC_SENSE_LEVEL;
	else
		x |= OPENPIC_SENSE_EDGE;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_disable_irq(irq)
	int irq;
{
	u_int x;

	/* skip invalid irqs */
	if (irq >= PIC_OFFSET)
		irq -= PIC_OFFSET;

	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x |= OPENPIC_IMASK;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
i8259_set_irq_mask(void)
{
	if (icu2_val != 0xff) {
		/* Turn on the second IC */
		icu1_val &= ~(1 << IRQ_SLAVE);
	} else {
		icu1_val |= (1 << IRQ_SLAVE);
	}

	outb(IO_ICU1 + 1, icu1_val);
	outb(IO_ICU2 + 1, icu2_val);
	outb(IO_ELCR1, elcr1_val);
	outb(IO_ELCR2, elcr2_val);
}

void
i8259_disable_irq(irq)
	int irq;
{
#ifdef DIAGNOSTIC
	/* skip invalid irqs */
	if (irq < 0 || irq >= PIC_OFFSET)
		return;
#endif

	if (irq < 8) {
		icu1_val |= 1 << irq;
		elcr1_val &= ~(1 << irq);
	} else {
		irq -= 8;
		icu2_val |= 1 << irq;
		elcr2_val &= ~(1 << irq);
	}
}

void
i8259_enable_irq(irq, type)
	int irq, type;
{
#ifdef DIAGNOSTIC
	/* skip invalid irqs */
	if (irq < 0 || irq >= PIC_OFFSET)
		return;
#endif

	if (irq < 8) {
		icu1_val &= ~(1 << irq);
		if (type == IST_LEVEL)
			elcr1_val |= (1 << irq);
		else
			elcr1_val &= ~(1 << irq);
	} else {
		irq -= 8;
		icu2_val &= ~(1 << irq);
		if (type == IST_LEVEL)
			elcr2_val |= (1 << irq);
		else
			elcr2_val &= ~(1 << irq);
	}
}

static __inline void
i8259_eoi(int irq)
{
#ifdef DIAGNOSTIC
	/* skip invalid irqs */
	if (irq < 0 || irq >= PIC_OFFSET)
		return;
#endif
	if (irq < 8)
		outb(IO_ICU1, 0x60 | irq);
	else {
		outb(IO_ICU2, 0x60 | (irq - 8));
		outb(IO_ICU1, 0x60 | IRQ_SLAVE);
	}
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
openpic_iack(cpu)
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
i8259_init(void)
{
	/* initialize 8259's */
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU1+1, ICU_OFFSET);	/* starting at this vector index */
	outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
	outb(IO_ICU1+1, 1);		/* 8086 mode */
	outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	/* init interrupt controller 2 */
	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+1, ICU_OFFSET+8);	/* staring at this vector index */
	outb(IO_ICU2+1, IRQ_SLAVE);
	outb(IO_ICU2+1, 1);		/* 8086 mode */
	outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
}

int
i8259_intr(void)
{
	int irq;

	/*
	 * Perform an interrupt acknowledge cycle on controller 1
	 */
	outb(IO_ICU1, 0x0c);
	irq = inb(IO_ICU1) & 7;

	if (irq == IRQ_SLAVE) {
		/*
		 * Interrupt is cascaded so perform interrupt
		 * acknowledge on controller 2
		 */
		outb(IO_ICU2, 0x0c);
		irq = (inb(IO_ICU2) & 7) + 8;
	} else if (irq == 7) {
		/*
		 * This may be a spurious interrupt
		 *
		 * Read the interrupt status register. If the most
		 * significant bit is not set then there is no valid
		 * interrupt
		 */
		outb(IO_ICU1, 0x0b);
		if ((inb(IO_ICU1) & 0x80) == 0) {
#ifdef DIAGNOSTIC
			printf("spurious interrupt on ICU1\n");
#endif
			return PIC_SPURIOUS;
		}
	}

	return (ICU_OFFSET + irq);
}

void
ext_intr_openpic()
{
	int irq, realirq;
	int r_imen;
	int pcpl, ocpl;
	struct intrhand *ih;

	pcpl = cpl;

	realirq = openpic_iack(0);

	while (realirq != PIC_SPURIOUS) {
		if (realirq == 0x00) {
			/*
			 * Interrupt from the PCI/ISA bridge. PCI interrupts
			 * are shadowed on the ISA PIC for compatibility with
			 * MVME1600, so simply handle the ISA PIC.
			 */
			realirq = i8259_intr();
			openpic_eoi(0);
			if (realirq == PIC_SPURIOUS)
				break;
		} else {
			realirq += PIC_OFFSET;
		}

		irq = virq[realirq];
		intrcnt[realirq]++;

		/* XXX check range */

		r_imen = 1 << irq;

		if ((pcpl & r_imen) != 0) {
			ipending |= r_imen;		/* Masked! Mark this as pending */
			if (realirq >= PIC_OFFSET) {
				openpic_disable_irq(realirq);
				openpic_eoi(0);
			} else {
				i8259_disable_irq(realirq);
				i8259_set_irq_mask();
				i8259_eoi(realirq);
			}
		} else {
			if (realirq >= PIC_OFFSET) {
				openpic_disable_irq(realirq);
			} else {
				i8259_disable_irq(realirq);
				i8259_set_irq_mask();
			}

			ocpl = splraise(intrmask[irq]);

			ih = intrhand[irq];
			while (ih) {
				(*ih->ih_fun)(ih->ih_arg);
				ih = ih->ih_next;
			}

			uvmexp.intrs++;
			__asm__ volatile("":::"memory");
			cpl = ocpl;
			__asm__ volatile("":::"memory");
#if 0
			evirq[realirq].ev_count++;
#endif
			if (realirq >= PIC_OFFSET) {
				openpic_eoi(0);
				openpic_enable_irq(realirq, intrtype[irq]);
			} else {
				i8259_eoi(realirq);
				i8259_enable_irq(realirq, intrtype[irq]);
				i8259_set_irq_mask();
			}
		}

		realirq = openpic_iack(0);
	}
	ppc_intr_enable(1);

	splx(pcpl);	 /* Process pendings. */
}

void
openpic_initirq(int irq, int pol, int sense)
{
	u_int x;

	x = (irq & OPENPIC_VECTOR_MASK);
	x |= OPENPIC_IMASK;
	x |= (pol ? OPENPIC_POLARITY_POSITIVE : OPENPIC_POLARITY_NEGATIVE);
	x |= (sense ? OPENPIC_SENSE_LEVEL : OPENPIC_SENSE_EDGE);
	x |= 8 << OPENPIC_PRIORITY_SHIFT;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_init()
{
	int irq;
	u_int x;

	/* disable all interrupts and init hwirq[] */
	for (irq = 0; irq < ICU_LEN; irq++) {
		hwirq[irq] = -1;
		intrtype[irq] = IST_NONE;
		intrmask[irq] = 0;
		intrlevel[irq] = 0;
		intrhand[irq] = NULL;
		openpic_write(OPENPIC_SRC_VECTOR(irq), OPENPIC_IMASK);
	}
	openpic_set_priority(0, 15);

	/* we don't need 8259 pass through mode */
	x = openpic_read(OPENPIC_CONFIG);
	x |= OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
	openpic_write(OPENPIC_CONFIG, x);

	/* send all interrupts to cpu 0 */
	for (irq = 0; irq < ICU_LEN; irq++)
		openpic_write(OPENPIC_SRC_DEST(irq), CPU(0));

	/* special case for intr src 0 */
	openpic_initirq(0, 1, 0);
	for (irq = 1; irq < ICU_LEN; irq++) {
		openpic_initirq(irq, 0, 1);
	}

	/* XXX set spurious intr vector */
#if 0
	openpic_write(OPENPIC_SPURIOUS_VECTOR, 0xFF);
#endif

	/* unmask interrupts for cpu 0 */
	openpic_set_priority(0, 0);

	/* clear all pending interrunts */	/* < ICU_LEN ? */
	for (irq = 0; irq < PIC_OFFSET; irq++) {
		openpic_iack(0);
		openpic_eoi(0);
	}

	for (irq = 0; irq < PIC_OFFSET; irq++) {	/* < ICU_LEN ? */
		i8259_disable_irq(irq);
		openpic_disable_irq(irq);
	}

	i8259_set_irq_mask();

	install_extint(ext_intr_openpic);
}
