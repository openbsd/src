/*	$OpenBSD: openpic.c,v 1.6 2001/11/06 22:45:54 miod Exp $	*/

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

#if 0
#define OP_DEBUG
#endif 

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <uvm/uvm.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <mvmeppc/dev/openpicreg.h>
#include <mvmeppc/dev/ravenvar.h>
#include <mvmeppc/dev/ravenreg.h>

#define ICU_LEN 32
#define LEGAL_IRQ(x) ((x >= 0) && (x < ICU_LEN))
#define IO_ICU1	(RAVEN_P_ISA_IO_SPACE + 0x20)
#define IO_ICU2	(RAVEN_P_ISA_IO_SPACE + 0xA0)
#define IO_ELCR1	(RAVEN_P_ISA_IO_SPACE + 0x4D0)
#define IO_ELCR2	(RAVEN_P_ISA_IO_SPACE + 0x4D1)
#define IRQ_SLAVE	2
#define ICU_OFFSET	0
#define PIC_OFFSET	16

unsigned char icu1_val = 0xff;
unsigned char icu2_val = 0xff;
unsigned char elcr1_val = 0x00;
unsigned char elcr2_val = 0x00;

#define SET_ICUS()	(outb(IO_ICU1 + 1, imen), outb(IO_ICU2 + 1, imen >> 8))

static int intrtype[ICU_LEN], intrmask[ICU_LEN], intrlevel[ICU_LEN];
static struct intrhand *intrhand[ICU_LEN] = { 0};
static int hwirq[ICU_LEN], virq[ICU_LEN];
unsigned int imen	/* = 0xffffffff */; /* XXX */
static int virq_max = 0;

struct evcnt evirq[ICU_LEN];

static int fakeintr __P((void *));
static char *intr_typename(int type);
static void intr_calculatemasks();
static __inline int cntlzw(int x);
static int mapirq(int irq);
void openpic_enable_irq_mask(int irq_mask);

static struct raven_reg *ravenp = (struct raven_reg *)NULL;

#define HWIRQ_MAX 27
#define HWIRQ_MASK 0x0fffffff

static   __inline u_int openpic_read __P((int));
static   __inline void openpic_write __P((int, u_int));
void  openpic_enable_irq __P((int, int));
void  openpic_disable_irq __P((int));
void  openpic_init();
void  openpic_set_priority __P((int, int));
void  openpic_set_vec_pri __P((int, int));
static   __inline int openpic_read_irq __P((int));
static   __inline void openpic_eoi __P((int));

void  i8259_init __P((void));
int   i8259_intr __P((void));
void  i8259_enable_irq __P((int, int));
void  i8259_disable_irq __P((int));
void  *i8259_intr_establish( void * lcv, int irq, int type, int level,
									  int (*ih_fun) __P((void *)), void *ih_arg, char *name);

struct openpic_softc {
	struct device sc_dev;
};

int   openpic_match __P((struct device *parent, void *cf, void *aux));
void  openpic_attach __P((struct device *, struct device *, void *));
void  openpic_do_pending_int();
void  ext_intr_openpic();

struct cfattach openpic_ca = { 
	sizeof(struct openpic_softc),
	openpic_match,
	openpic_attach
};

struct cfdriver openpic_cd = {
	NULL, "openpic", DV_DULL
};

struct pci_route {
	int pci;
	int openpic;
} pci_routes[] = {
	{ 10, 2 },
	{ 11, 4 },
	{ 14, 3 },
	{ 15, 5 },
	{ 0, 0 }
};

static int isaintrs = 0;

int
openpic_match(parent, cf, aux) 
struct device *parent;
void *cf;
void *aux;
{
	/* We must be a child of the raven device */
	if (strcmp(parent->dv_cfdata->cf_driver->cd_name, "raven") != 0)
		return (0);
	/* don't attach more than once. */
	if (ravenp != (struct raven_reg *)NULL) {
#ifdef DIAGNOSTIC
		printf("openpic: trying to attach more than once!");
#endif
		return (0);
	}
	/* If there is a raven, then there is a mpic! */
	return 1;
}

u_int8_t *interrupt_reg;
typedef void  (void_f) (void);
extern void_f *pending_int_f;
static int abort_switch (void *arg);
static int i8259_dummy (void *arg);

typedef int mac_intr_handle_t;

typedef void     *(intr_establish_t) __P((void *, mac_intr_handle_t,
														int, int, int (*func)(void *), void *, char *));
typedef void     (intr_disestablish_t) __P((void *, void *));

static vaddr_t openpic_base;
void * openpic_intr_establish( void * lcv, int irq, int type, int level,
										 int (*ih_fun) __P((void *)), void *ih_arg, char *name);
void openpic_intr_disestablish( void *lcp, void *arg);
void openpic_collect_preconf_intr();

void
openpic_attach(parent, self, aux)
struct device *parent, *self;
void *aux;
{
	extern intr_establish_t *intr_establish_func;
	extern intr_disestablish_t *intr_disestablish_func;
#if 0
	extern intr_establish_t *mac_intr_establish_func;
	extern intr_disestablish_t *mac_intr_disestablish_func;
#endif 
	openpic_base = (vaddr_t)mapiodev(MPCIC_REG, 0x22000);

	printf(": version 0x%x", openpic_read(OPENPIC_FEATURE) & 0xFF);

	i8259_init();
	openpic_init();

	pending_int_f = openpic_do_pending_int;
	intr_establish_func  = i8259_intr_establish;
	intr_disestablish_func  = openpic_intr_disestablish;
#if 0
	mac_intr_establish_func  = openpic_intr_establish;
	mac_intr_disestablish_func  = openpic_intr_disestablish;
#endif 
	install_extint(ext_intr_openpic);

#if 1
	openpic_collect_preconf_intr();
#endif


#if 1
	openpic_intr_establish(parent, 0x00, IST_LEVEL, IPL_HIGH,
							  i8259_dummy, (void *)0x00, "8259 Interrupt");
	i8259_intr_establish(parent, 0x08, IST_EDGE, IPL_HIGH,
								abort_switch, (void *)0x08, "abort button");
#endif
	
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
abort_switch (void *arg)
{
#ifdef DDB
	printf("Abort button pressed, entering debugger.\n");
	Debugger();
#else
	printf("Abort button pressed, debugger not available.\n");
#endif
	return 1;
}

static int
i8259_dummy (void *arg)
{
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
i8259_intr_establish(lcv, irq, type, level, ih_fun, ih_arg, name)
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
	printf("i8259_intr_establish, %d, %s", irq, (type == IST_EDGE) ? "EDGE":"LEVEL"));
#endif
	isaintrs++;
	irq = mapirq(irq + ICU_OFFSET);
#if 0
	printf("vI %d ", irq);
#endif

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("i8259_intr_establish: can't malloc handler info");

	if (!LEGAL_IRQ(irq) || type == IST_NONE)
		panic("i8259_intr_establish: bogus irq or type");

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
	struct pci_route *pr;
	extern int cold;

	fakehand.ih_next = NULL;
	fakehand.ih_fun  = fakeintr;

#if 0
	printf("mac_intr_establish, hI %d L %d ", irq, type);
#endif

	pr = pci_routes;
	while (pr->pci !=0) {
		irq = (pr->pci == irq) ? pr->openpic : irq;
		pr++;
	}

	irq = mapirq(irq + PIC_OFFSET);
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
#ifdef OP_DEBUG
	printf("intr_calculatemasks() ");
#endif 
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
				if (hwirq[irq] < PIC_OFFSET)
					i8259_enable_irq(hwirq[irq], intrtype[irq]);
				else
					openpic_enable_irq(hwirq[irq], intrtype[irq]);
			} else {
				if (hwirq[irq] >= PIC_OFFSET)
					openpic_disable_irq(hwirq[irq]);
				else
					i8259_disable_irq(hwirq[irq]);
			}
		}
	}
#if 0
	i8259_enable_irq(2, IST_EDGE);
#endif 
}
/*
 * Map 64 irqs into 32 (bits).
 */
static int
mapirq(irq)
int irq;
{
	int v;

	if (irq < 0 || irq >= ICU_LEN)
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

#ifdef OP_DEBUG
	printf("openpic_do_pending_int()\n");
#endif 
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
		while (ih) {
			(*ih->ih_fun)(ih->ih_arg);
			ih = ih->ih_next;
		}

		evirq[hwirq[irq]].ev_count++;
	}

	/*out32rb(INT_ENABLE_REG, ~imen);*/

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
#ifdef OP_DEBUG
	printf("openpic_enable_irq_mask()\n");
#endif 
	for ( irq = 0; irq <= virq_max; irq++) {
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
	}
}

void
openpic_enable_irq(irq, type)
int irq;
int type;
{
	u_int x;
	/* skip invalid irqs */
	if (irq == -1)
		return;
	if (irq >= PIC_OFFSET)
		irq -= PIC_OFFSET;
#ifdef OP_DEBUG
	printf("enabeling irq %d, %s, val = 0x%x\n", irq, (type == IST_EDGE) ? "EDGE":"LEVEL", openpic_read(OPENPIC_SRC_VECTOR(irq)));
#endif 

	while((x = openpic_read(OPENPIC_SRC_VECTOR(irq))) & OPENPIC_ACTIVITY){
		x = openpic_read_irq(0);
		openpic_eoi(0);
#ifdef OP_DEBUG
		printf("x=0x%x\n", x);
#endif 
	}
	
	x &= ~(OPENPIC_IMASK|OPENPIC_SENSE_LEVEL|OPENPIC_SENSE_EDGE|
			 OPENPIC_POLARITY_POSITIVE);
#if 1
	if (irq == 0) {
		x |= OPENPIC_POLARITY_POSITIVE;
	}
#endif 
	if (type == IST_LEVEL) {
		x |= OPENPIC_SENSE_LEVEL;
	} else {
		x |= OPENPIC_SENSE_EDGE;
	}
#ifdef OP_DEBUG
	printf("enabeling irq %d, %s, %s, val 0x%08x\n", irq, (type == IST_EDGE) ? "EDGE":"LEVEL", (x & OPENPIC_POLARITY_POSITIVE) ? "H":"L", x);
#endif 

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
#ifdef OP_DEBUG
	printf("disabeling irq %d, val 0x%08x\n", irq, x);
#endif 

	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void 
i8259_set_irq_mask(void)
{
	if (icu2_val != 0xFF) {
		/* Turn on the second IC */
#ifdef OP_DEBUG
		printf("turning on ICU2\n");
#endif 
		icu1_val &= ~(1 << 2);
	} else {
		icu1_val |= (1 << 2);
	}

	outb(IO_ICU1 + 1, icu1_val);
	outb(IO_ICU2 + 1, icu2_val);
	outb(IO_ELCR1, elcr1_val);
	outb(IO_ELCR2, elcr2_val);
#ifdef OP_DEBUG
	printf("ICU  %x-%x\n", icu2_val, icu1_val);
	printf("ELCR %x-%x\n", elcr2_val, elcr1_val);
#endif 
}

void
i8259_disable_irq(irq)
int irq;
{
	if (irq == -1)
		return;
	if (irq < 8)
		icu1_val |= 1 << irq;
	else
		icu2_val	|= 1 << (irq - 8);
	i8259_set_irq_mask();
#ifdef OP_DEBUG
	printf("disabeling isa irq %d\n", irq);
#endif 
}

void 
i8259_enable_irq(irq, type)
int irq, type; 
{
	/* skip invalid irqs */
	if (irq == -1)
		return;
	if ( irq < 8 ){
		icu1_val &= ~(1 << irq);
		if (type == IST_LEVEL) {
			elcr1_val |= (1 << irq);
		} else {
			elcr1_val &= ~(1 << irq);
		}
	} else {
		icu2_val	&= ~(1 << (irq - 8));
		if (type == IST_LEVEL) {
			elcr2_val |= (1 << (irq - 8));
		} else {
			elcr2_val &= ~(1 << (irq - 8));
		}
	}
	i8259_set_irq_mask();
#ifdef OP_DEBUG
	printf("enabeling isa irq %d, %s\n", irq, (type == IST_EDGE) ? "EDGE":"LEVEL");
#endif 

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

void i8259_init(void)
{
#if 0
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
#endif 
}

int i8259_intr(void)
{
	int irq;

	/*
	 * Perform an interrupt acknowledge cycle on controller 1
	 */
	outb(IO_ICU1, 0x0C);
	irq = inb(IO_ICU1) & 7;
#ifdef OP_DEBUG
	printf("isa intr = %d\n", irq);
#endif 
	if (irq == 2) {
		/*
		 * Interrupt is cascaded so perform interrupt
		 * acknowledge on controller 2
		 */
		outb(IO_ICU2, 0x0C);
		irq = (inb(IO_ICU2) & 7) + 8;
	} else if (irq==7) {
		/*
		 * This may be a spurious interrupt
		 *
		 * Read the interrupt status register. If the most
		 * significant bit is not set then there is no valid
* interrupt
*/
		outb(IO_ICU1, 0x0B);
		if (~inb(IO_ICU1)&0x80)
			return 0xFF;
	}
	return (ICU_OFFSET + irq);
}

void
ext_intr_openpic()
{
	int irq, realirq;
	int r_imen;
	int pcpl;
	struct intrhand *ih;

#ifdef OP_DEBUG
	printf("Interrupt!\n");
#endif 
	pcpl = splhigh();			/* Turn off all */

	realirq = openpic_read_irq(0);
#ifdef OP_DEBUG
	printf("irq %d\n", realirq);
#endif 

	while (realirq != 0xFF) {
		if (realirq == 0x00) {
			realirq = i8259_intr();
#ifdef OP_DEBUG
			printf("irq2 %d\n", realirq);
#endif 
			openpic_eoi(0);
			if (realirq == 0xFF)
				continue;
		}

		irq = virq[realirq];
		intrcnt[realirq]++;

		/* XXX check range */

		r_imen = 1 << irq;

		if ((pcpl & r_imen) != 0) {
			ipending |= r_imen;		/* Masked! Mark this as pending */
			if (realirq >= ICU_OFFSET)
				i8259_disable_irq(realirq);
			else
				openpic_disable_irq(realirq);
		} else {
			ih = intrhand[irq];
			while (ih) {
				(*ih->ih_fun)(ih->ih_arg);
				ih = ih->ih_next;
			}
			uvmexp.intrs++;
			evirq[realirq].ev_count++;
		}

		openpic_eoi(0);

		realirq = openpic_read_irq(0);
	}

	splx(pcpl);		 /* Process pendings. */
}

void openpic_set_vec_pri(int irq, int pri)
{
	u_int x;
	x = openpic_read(OPENPIC_SRC_VECTOR(irq));
	x &= ~OPENPIC_PRIORITY_MASK;
	x |= pri << OPENPIC_PRIORITY_SHIFT;
	openpic_write(OPENPIC_SRC_VECTOR(irq), x);
}

void openpic_initirq(int irq, int pri, int vec, int pol, int sense)
{
	u_int x;
	x = (vec & OPENPIC_VECTOR_MASK);
	x |= OPENPIC_IMASK;
	x |= (pol ? OPENPIC_POLARITY_POSITIVE : OPENPIC_POLARITY_NEGATIVE);
	x |= (sense ? OPENPIC_SENSE_LEVEL : OPENPIC_SENSE_EDGE);
	x |= pri << OPENPIC_PRIORITY_SHIFT;
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
	openpic_initirq(0, 8, 0, 1, 0);

	for (irq = 1; irq < ICU_LEN; irq++) {
		openpic_initirq(irq, 8, irq, 0, 1);
	}

	/* XXX set spurious intr vector */
	openpic_write(OPENPIC_SPURIOUS_VECTOR, 0xFF);   

	/* unmask interrupts for cpu 0 */
	openpic_set_priority(0, 0);

	/* clear all pending interrunts */
	for (irq = 0; irq < PIC_OFFSET; irq++) {
		openpic_read_irq(0);
		openpic_eoi(0);
	}

	for (irq = 0; irq < PIC_OFFSET; irq++) {
		i8259_disable_irq(irq);
		openpic_disable_irq(irq);
	}

	install_extint(ext_intr_openpic);
}
