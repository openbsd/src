/*	$OpenBSD: openpic.c,v 1.63 2011/01/08 18:10:22 deraadt Exp $	*/

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

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <machine/powerpc.h>
#include <macppc/dev/openpicreg.h>
#include <dev/ofw/openfirm.h>

#define ICU_LEN 128
#define LEGAL_IRQ(x) ((x >= 0) && (x < ICU_LEN))

int o_intrtype[ICU_LEN], o_intrmaxlvl[ICU_LEN];
struct intrhand *o_intrhand[ICU_LEN] = { 0 };
int o_hwirq[ICU_LEN], o_virq[ICU_LEN];
int o_virq_max;

static int fakeintr(void *);
static char *intr_typename(int type);
void openpic_calc_mask(void);
static __inline int cntlzw(int x);
static int mapirq(int irq);
void openpic_enable_irq_mask(int irq_mask);

#define HWIRQ_MAX (31 - (SI_NQUEUES + 1))
#define HWIRQ_MASK (0xffffffff >> (SI_NQUEUES + 1))

/* IRQ vector used for inter-processor interrupts. */
#define IPI_VECTOR_NOP	64
#define IPI_VECTOR_DDB	65
#ifdef MULTIPROCESSOR
static struct evcount ipi_ddb[PPC_MAXPROCS];
static struct evcount ipi_nop[PPC_MAXPROCS];
static int ipi_nopirq = IPI_VECTOR_NOP;
static int ipi_ddbirq = IPI_VECTOR_DDB;
#endif

static __inline u_int openpic_read(int);
static __inline void openpic_write(int, u_int);
void openpic_set_enable_irq(int, int);
void openpic_enable_irq(int);
void openpic_disable_irq(int);
void openpic_init(void);
void openpic_set_priority(int, int);
void openpic_ipi_ddb(void);
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

	if (strcmp(ca->ca_name, "interrupt-controller") != 0 &&
	    strcmp(ca->ca_name, "mpic") != 0)
		return 0;

	OF_getprop(ca->ca_node, "device_type", type, sizeof(type));
	if (strcmp(type, "open-pic") != 0)
		return 0;

	if (ca->ca_nreg < 8)
		return 0;

	return 1;
}

typedef void  (void_f) (void);
extern void_f *pending_int_f;

vaddr_t openpic_base;
void * openpic_intr_establish( void * lcv, int irq, int type, int level,
	int (*ih_fun)(void *), void *ih_arg, const char *name);
void openpic_intr_disestablish( void *lcp, void *arg);
#ifdef MULTIPROCESSOR
intr_send_ipi_t openpic_send_ipi;
#endif
void openpic_collect_preconf_intr(void);
int openpic_big_endian;

void
openpic_attach(struct device *parent, struct device  *self, void *aux)
{
	struct confargs *ca = aux;
	u_int32_t reg;

	reg = 0;
	if (OF_getprop(ca->ca_node, "big-endian", &reg, sizeof reg) == 0)
		openpic_big_endian = 1;

	openpic_base = (vaddr_t) mapiodev (ca->ca_baseaddr +
			ca->ca_reg[0], 0x40000);

	printf(": version 0x%x %s endian", openpic_read(OPENPIC_VENDOR_ID),
		openpic_big_endian ? "big" : "little" );

	openpic_init();

	pending_int_f = openpic_do_pending_int;
	intr_establish_func  = openpic_intr_establish;
	intr_disestablish_func  = openpic_intr_disestablish;
	mac_intr_establish_func  = openpic_intr_establish;
	mac_intr_disestablish_func  = openpic_intr_disestablish;
#ifdef MULTIPROCESSOR
	intr_send_ipi_func = openpic_send_ipi;
#endif
	install_extint(ext_intr_openpic);

#if 1
	openpic_collect_preconf_intr();
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
    int (*ih_fun)(void *), void *ih_arg, const char *name)
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

	openpic_calc_mask();

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, name, &o_hwirq[irq]);
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

	openpic_calc_mask();

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

void
openpic_calc_mask()
{
	int irq;
	struct intrhand *ih;
	int i;

	/* disable all openpic interrupts */
	openpic_set_priority(0, 15);

	for (irq = 0; irq < ICU_LEN; irq++) {
		int max = IPL_NONE;
		int min = IPL_HIGH;
		int reg;
		if (o_virq[irq] != 0) {
			for (ih = o_intrhand[o_virq[irq]]; ih;
			    ih = ih->ih_next) {
				if (ih->ih_level > max)
					max = ih->ih_level;
				if (ih->ih_level < min)
					min = ih->ih_level;
			}
		}

		o_intrmaxlvl[irq] = max;

		/* adjust priority if it changes */
		reg = openpic_read(OPENPIC_SRC_VECTOR(irq));
		if (max != ((reg >> OPENPIC_PRIORITY_SHIFT) & 0xf)) {
			openpic_write(OPENPIC_SRC_VECTOR(irq),
				(reg & ~(0xf << OPENPIC_PRIORITY_SHIFT)) |
				(max << OPENPIC_PRIORITY_SHIFT) );
		}

		if (max == IPL_NONE)
			min = IPL_NONE; /* Interrupt not enabled */

		if (o_virq[irq] != 0) {
			/* Enable (dont mask) interrupts at lower levels */ 
			for (i = IPL_NONE; i < min; i++)
				cpu_imask[i] &= ~(1 << o_virq[irq]);
			for (; i <= IPL_HIGH; i++)
				cpu_imask[i] |= (1 << o_virq[irq]);
		}
	}

	/* restore interrupts */
	openpic_set_priority(0, 0);

	for (i = IPL_NONE; i <= IPL_HIGH; i++) {
		if (i > IPL_NONE)
			cpu_imask[i] |= SINT_ALLMASK;
		if (i >= IPL_CLOCK)
			cpu_imask[i] |= SPL_CLOCKMASK;
	}
	cpu_imask[IPL_HIGH] = 0xffffffff;
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

void openpic_do_pending_softint(int pcpl);

void
openpic_do_pending_int()
{
	struct cpu_info *ci = curcpu();
	struct intrhand *ih;
	int irq;
	int pcpl;
	int hwpend;
	int pri, pripending;
	int s;

	if (ci->ci_iactive & CI_IACTIVE_PROCESSING_HARD)
		return;

	atomic_setbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_HARD);
	s = ppc_intr_disable();
	pcpl = ci->ci_cpl;

	hwpend = ci->ci_ipending & ~pcpl;	/* Do now unmasked pendings */
	hwpend &= HWIRQ_MASK;
	while (hwpend) {
		/* this still doesn't handle the interrupts in priority order */
		for (pri = IPL_HIGH; pri >= IPL_NONE; pri--) {
			pripending = hwpend & ~cpu_imask[pri];
			if (pripending == 0)
				continue;
			irq = 31 - cntlzw(pripending);
			ci->ci_ipending &= ~(1 << irq);
			ci->ci_cpl = cpu_imask[o_intrmaxlvl[o_hwirq[irq]]];
			openpic_enable_irq_mask(~ci->ci_cpl);
			ih = o_intrhand[irq];
			while(ih) {
				ppc_intr_enable(1);

				KERNEL_LOCK();
				if ((*ih->ih_fun)(ih->ih_arg))
					ih->ih_count.ec_count++;
				KERNEL_UNLOCK();

				(void)ppc_intr_disable();
				
				ih = ih->ih_next;
			}
		}
		hwpend = ci->ci_ipending & ~pcpl;/* Catch new pendings */
		hwpend &= HWIRQ_MASK;
	}
	ci->ci_cpl = pcpl | SINT_ALLMASK;
	openpic_enable_irq_mask(~ci->ci_cpl);
	atomic_clearbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_HARD);

	openpic_do_pending_softint(pcpl);

	ppc_intr_enable(s);
}

void
openpic_do_pending_softint(int pcpl)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_iactive & CI_IACTIVE_PROCESSING_SOFT)
		return;

	atomic_setbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_SOFT);

	do {
		if((ci->ci_ipending & SINT_CLOCK) & ~pcpl) {
			ci->ci_ipending &= ~SINT_CLOCK;
			ci->ci_cpl = SINT_CLOCK|SINT_NET|SINT_TTY;
			ppc_intr_enable(1);
			softintr_dispatch(SI_SOFTCLOCK);
			ppc_intr_disable();
			continue;
		}
		if((ci->ci_ipending & SINT_NET) & ~pcpl) {
			ci->ci_ipending &= ~SINT_NET;
			ci->ci_cpl = SINT_NET|SINT_TTY;
			ppc_intr_enable(1);
			softintr_dispatch(SI_SOFTNET);
			ppc_intr_disable();
			continue;
		}
		if((ci->ci_ipending & SINT_TTY) & ~pcpl) {
			ci->ci_ipending &= ~SINT_TTY;
			ci->ci_cpl = SINT_TTY;
			ppc_intr_enable(1);
			softintr_dispatch(SI_SOFTTTY);
			ppc_intr_disable();
			continue;
		}
	} while ((ci->ci_ipending & SINT_ALLMASK) & ~pcpl);
	ci->ci_cpl = pcpl;	/* Don't use splx... we are here already! */

	atomic_clearbits_int(&ci->ci_iactive, CI_IACTIVE_PROCESSING_SOFT);
}

u_int
openpic_read(int reg)
{
	char *addr = (void *)(openpic_base + reg);

	if (openpic_big_endian)
		return in32(addr);
	else
		return in32rb(addr);
}

void
openpic_write(int reg, u_int val)
{
	char *addr = (void *)(openpic_base + reg);

	if (openpic_big_endian)
		out32(addr, val);
	else
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

#ifdef MULTIPROCESSOR

void
openpic_send_ipi(struct cpu_info *ci, int id)
{
	switch (id) {
	case PPC_IPI_NOP:
		id = 0;
		break;
	case PPC_IPI_DDB:
		id = 1;
		break;
	default:
		panic("invalid ipi send to cpu %d %d", ci->ci_cpuid, id);
	}
		
		
	openpic_write(OPENPIC_IPI(curcpu()->ci_cpuid, id), 1 << ci->ci_cpuid);
}

#endif

void
ext_intr_openpic()
{
	struct cpu_info *ci = curcpu();
	int irq, realirq;
	int r_imen;
	int pcpl, ocpl;
	struct intrhand *ih;

	pcpl = ci->ci_cpl;

	realirq = openpic_read_irq(ci->ci_cpuid);

	while (realirq != 255) {
#ifdef MULTIPROCESSOR
		if (realirq == IPI_VECTOR_NOP) {
			ipi_nop[ci->ci_cpuid].ec_count++;
			openpic_eoi(ci->ci_cpuid);
			realirq = openpic_read_irq(ci->ci_cpuid);
			continue;
		}
		if (realirq == IPI_VECTOR_DDB) {
			ipi_ddb[ci->ci_cpuid].ec_count++;
			openpic_eoi(ci->ci_cpuid);
			openpic_ipi_ddb();
			realirq = openpic_read_irq(ci->ci_cpuid);
			continue;
		}
#endif

		irq = o_virq[realirq];

		/* XXX check range */

		r_imen = 1 << irq;

		if ((pcpl & r_imen) != 0) {
			/* Masked! Mark this as pending. */
			ci->ci_ipending |= r_imen;
			openpic_enable_irq_mask(~cpu_imask[o_intrmaxlvl[realirq]]);
			openpic_eoi(ci->ci_cpuid);
		} else {
			openpic_enable_irq_mask(~cpu_imask[o_intrmaxlvl[realirq]]);
			openpic_eoi(ci->ci_cpuid);
			ocpl = splraise(cpu_imask[o_intrmaxlvl[realirq]]);

			ih = o_intrhand[irq];
			while (ih) {
				ppc_intr_enable(1);

				KERNEL_LOCK();
				if ((*ih->ih_fun)(ih->ih_arg))
					ih->ih_count.ec_count++;
				KERNEL_UNLOCK();

				(void)ppc_intr_disable();
				ih = ih->ih_next;
			}

			uvmexp.intrs++;
			__asm__ volatile("":::"memory"); /* don't reorder.... */
			ci->ci_cpl = ocpl;
			__asm__ volatile("":::"memory"); /* don't reorder.... */
			openpic_enable_irq_mask(~pcpl);
		}

		realirq = openpic_read_irq(ci->ci_cpuid);
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

#ifdef MULTIPROCESSOR
	/* Set up inter-processor interrupts. */
	/* IPI0 - NOP */
	x = openpic_read(OPENPIC_IPI_VECTOR(0));
	x &= ~(OPENPIC_IMASK | OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK);
	x |= (15 << OPENPIC_PRIORITY_SHIFT) | IPI_VECTOR_NOP;
	openpic_write(OPENPIC_IPI_VECTOR(0), x);
	/* IPI1 - DDB */
	x = openpic_read(OPENPIC_IPI_VECTOR(1));
	x &= ~(OPENPIC_IMASK | OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK);
	x |= (15 << OPENPIC_PRIORITY_SHIFT) | IPI_VECTOR_DDB;
	openpic_write(OPENPIC_IPI_VECTOR(1), x);

	evcount_attach(&ipi_nop[0], "ipi_nop0", &ipi_nopirq);
	evcount_attach(&ipi_nop[1], "ipi_nop1", &ipi_nopirq);
	evcount_attach(&ipi_ddb[0], "ipi_ddb0", &ipi_ddbirq);
	evcount_attach(&ipi_ddb[1], "ipi_ddb1", &ipi_ddbirq);
#endif

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

void
openpic_ipi_ddb(void)
{
	Debugger();
}

