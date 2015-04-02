/*	$OpenBSD: openpic.c,v 1.78 2015/04/02 11:12:24 mpi Exp $	*/

/*-
 * Copyright (c) 2008 Dale Rahn <drahn@openbsd.org>
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
#include <sys/systm.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>
#include <ddb/db_var.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <dev/ofw/openfirm.h>

#include <macppc/dev/openpicreg.h>

#ifdef OPENPIC_DEBUG
#define DPRINTF(x...)	do { printf(x); } while(0)
#else
#define DPRINTF(x...)
#endif

#define ICU_LEN 128
int openpic_numirq = ICU_LEN;
#define LEGAL_IRQ(x) ((x >= 0) && (x < ICU_LEN))

int openpic_pri_share[IPL_NUM];

struct intrq openpic_handler[ICU_LEN];

struct openpic_softc {
	struct device sc_dev;
};

vaddr_t openpic_base;
int	openpic_big_endian;
struct	evcount openpic_spurious;
int	openpic_spurious_irq = 255;

int	openpic_match(struct device *parent, void *cf, void *aux);
void	openpic_attach(struct device *, struct device *, void *);

int	openpic_splraise(int);
int	openpic_spllower(int);
void	openpic_splx(int);

u_int	openpic_read(int reg);
void	openpic_write(int reg, u_int val);
void	openpic_enable_irq(int, int);
void	openpic_disable_irq(int);
void	openpic_init(void);
void	openpic_calc_mask(void);
void	openpic_set_priority(int, int);
void	*openpic_intr_establish(void *, int, int, int, int (*)(void *), void *,
	    const char *);
void	openpic_intr_disestablish( void *lcp, void *arg);
void	openpic_collect_preconf_intr(void);
void	openpic_ext_intr(void);

#ifdef MULTIPROCESSOR
void	openpic_ipi_ddb(void);

/* IRQ vector used for inter-processor interrupts. */
#define IPI_VECTOR_NOP	64
#define IPI_VECTOR_DDB	65

static struct evcount ipi_ddb[PPC_MAXPROCS];
static struct evcount ipi_nop[PPC_MAXPROCS];

static int ipi_nopirq = IPI_VECTOR_NOP;
static int ipi_ddbirq = IPI_VECTOR_DDB;

intr_send_ipi_t openpic_send_ipi;
#endif /* MULTIPROCESSOR */

const struct cfattach openpic_ca = {
	sizeof(struct openpic_softc), openpic_match, openpic_attach
};

struct cfdriver openpic_cd = {
	NULL, "openpic", DV_DULL
};

u_int
openpic_read(int reg)
{
	char *addr = (void *)(openpic_base + reg);

	asm volatile("eieio"::: "memory");
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
	asm volatile("eieio"::: "memory");
}

static inline int
openpic_read_irq(int cpu)
{
	return openpic_read(OPENPIC_IACK(cpu)) & OPENPIC_VECTOR_MASK;
}

static inline void
openpic_eoi(int cpu)
{
	openpic_write(OPENPIC_EOI(cpu), 0);
}

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

void
openpic_attach(struct device *parent, struct device  *self, void *aux)
{
	struct cpu_info *ci = curcpu();
	struct confargs *ca = aux;
	u_int32_t reg;

	reg = 0;
	if (OF_getprop(ca->ca_node, "big-endian", &reg, sizeof reg) == 0)
		openpic_big_endian = 1;

	openpic_base = (vaddr_t) mapiodev (ca->ca_baseaddr +
			ca->ca_reg[0], 0x40000);

	/* openpic may support more than 128 interupts but driver doesn't */
	openpic_numirq = ((openpic_read(OPENPIC_FEATURE) >> 16) & 0x7f)+1;

	printf(": version 0x%x feature %x %s",
	    openpic_read(OPENPIC_VENDOR_ID),
	    openpic_read(OPENPIC_FEATURE),
		openpic_big_endian ? "BE" : "LE" );

	openpic_init();

	intr_establish_func  = openpic_intr_establish;
	intr_disestablish_func  = openpic_intr_disestablish;
#ifdef MULTIPROCESSOR
	intr_send_ipi_func = openpic_send_ipi;
#endif

	ppc_smask_init();

	openpic_collect_preconf_intr();

	evcount_attach(&openpic_spurious, "spurious", &openpic_spurious_irq);

	ppc_intr_func.raise = openpic_splraise;
	ppc_intr_func.lower = openpic_spllower;
	ppc_intr_func.x = openpic_splx;

	openpic_set_priority(0, ci->ci_cpl);

	ppc_intr_enable(1);

	printf("\n");
}

/* Must be called with interrupt disable. */
static inline void
openpic_setipl(int newcpl)
{
	struct cpu_info *ci = curcpu();

	ci->ci_cpl = newcpl;
	openpic_set_priority(ci->ci_cpuid, newcpl);
}

int
openpic_splraise(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int ocpl = ci->ci_cpl;
	int s;

	newcpl = openpic_pri_share[newcpl];
	if (ocpl > newcpl)
		newcpl = ocpl;

	s = ppc_intr_disable();
	openpic_setipl(newcpl);
	ppc_intr_enable(s);

	return ocpl;
}

int
openpic_spllower(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int ocpl = ci->ci_cpl;

	openpic_splx(newcpl);

	return ocpl;
}

void
openpic_splx(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int intr, s;

	intr = ppc_intr_disable();
	openpic_setipl(newcpl);
	if (newcpl < IPL_SOFTTTY && (ci->ci_ipending & ppc_smask[newcpl])) {
		s = splsofttty();
		dosoftint(newcpl);
		openpic_setipl(s); /* no-overhead splx */
	}
	ppc_intr_enable(intr);
}

void
openpic_collect_preconf_intr()
{
	int i;
	for (i = 0; i < ppc_configed_intr_cnt; i++) {
		DPRINTF("\n\t%s irq %d level %d fun %p arg %p",
		    ppc_configed_intr[i].ih_what, ppc_configed_intr[i].ih_irq,
		    ppc_configed_intr[i].ih_level, ppc_configed_intr[i].ih_fun,
		    ppc_configed_intr[i].ih_arg);
		openpic_intr_establish(NULL, ppc_configed_intr[i].ih_irq,
		    IST_LEVEL, ppc_configed_intr[i].ih_level,
		    ppc_configed_intr[i].ih_fun, ppc_configed_intr[i].ih_arg,
		    ppc_configed_intr[i].ih_what);
	}
}

/*
 * Register an interrupt handler.
 */
void *
openpic_intr_establish(void *lcv, int irq, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *name)
{
	struct intrhand *ih;
	struct intrq *iq;
	int s;

	if (!LEGAL_IRQ(irq) || type == IST_NONE) {
		printf("%s: bogus irq %d or type %d", __func__, irq, type);
		return (NULL);
	}

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("%s: can't malloc handler info", __func__);
	iq = &openpic_handler[irq];

	switch (iq->iq_ist) {
	case IST_NONE:
		iq->iq_ist = type;
		break;
	case IST_EDGE:
		intr_shared_edge = 1;
		/* FALLTHROUGH */
	case IST_LEVEL:
		if (type == iq->iq_ist)
			break;
	case IST_PULSE:
		if (type != IST_NONE)
			panic("intr_establish: can't share %s with %s",
			    ppc_intr_typename(iq->iq_ist),
			    ppc_intr_typename(type));
		break;
	}

	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_level = level;
	ih->ih_irq = irq;

	evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	/*
	 * Append handler to end of list
	 */
	s = ppc_intr_disable();

	TAILQ_INSERT_TAIL(&iq->iq_list, ih, ih_list);
	openpic_calc_mask();

	ppc_intr_enable(s);

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
	struct intrq *iq = &openpic_handler[irq];
	int s;

	if (!LEGAL_IRQ(irq)) {
		printf("%s: bogus irq %d", __func__, irq);
		return;
	}

	/*
	 * Remove the handler from the chain.
	 */
	s = ppc_intr_disable();

	TAILQ_REMOVE(&iq->iq_list, ih, ih_list);
	openpic_calc_mask();

	ppc_intr_enable(s);

	evcount_detach(&ih->ih_count);
	free((void *)ih, M_DEVBUF, 0);

	if (TAILQ_EMPTY(&iq->iq_list))
		iq->iq_ist = IST_NONE;
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
	struct cpu_info *ci = curcpu();
	int irq;
	struct intrhand *ih;
	int i;

	/* disable all openpic interrupts */
	openpic_set_priority(ci->ci_cpuid, 15);

	for (i = IPL_NONE; i < IPL_NUM; i++) {
		openpic_pri_share[i] = i;
	}

	for (irq = 0; irq < openpic_numirq; irq++) {
		int maxipl = IPL_NONE;
		int minipl = IPL_HIGH;
		struct intrq *iq = &openpic_handler[irq];

		TAILQ_FOREACH(ih, &iq->iq_list, ih_list) {
			if (ih->ih_level > maxipl)
				maxipl = ih->ih_level;
			if (ih->ih_level < minipl)
				minipl = ih->ih_level;
		}

		if (maxipl == IPL_NONE) {
			minipl = IPL_NONE; /* Interrupt not enabled */

			openpic_disable_irq(irq);
		} else {
			for (i = minipl; i <= maxipl; i++) {
				openpic_pri_share[i] = maxipl;
			}
			openpic_enable_irq(irq, maxipl);
		}

		iq->iq_ipl = maxipl;
	}

	/* restore interrupts */
	openpic_set_priority(ci->ci_cpuid, ci->ci_cpl);
}

void
openpic_enable_irq(int irq, int pri)
{
	u_int x;
	struct intrq *iq = &openpic_handler[irq];

	x = irq;
	if (iq->iq_ist == IST_LEVEL)
		x |= OPENPIC_SENSE_LEVEL;
	else
		x |= OPENPIC_SENSE_EDGE;
	x |= OPENPIC_POLARITY_NEGATIVE;
	x |= pri << OPENPIC_PRIORITY_SHIFT;
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
	openpic_write(OPENPIC_CPU_PRIORITY(cpu), pri);
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

int openpic_irqnest[PPC_MAXPROCS];
int openpic_irqloop[PPC_MAXPROCS];
void
openpic_ext_intr()
{
	struct cpu_info *ci = curcpu();
	int irq, pcpl, ret;
	int maxipl = IPL_NONE;
	struct intrhand *ih;
	struct intrq *iq;
	int spurious;

	pcpl = ci->ci_cpl;

	openpic_irqloop[ci->ci_cpuid] = 0;
	irq = openpic_read_irq(ci->ci_cpuid);
	openpic_irqnest[ci->ci_cpuid]++;

	while (irq != 255) {
		openpic_irqloop[ci->ci_cpuid]++;
#ifdef OPENPIC_DEBUG
		if (openpic_irqloop[ci->ci_cpuid] > 20 ||
		    openpic_irqnest[ci->ci_cpuid] > 3) {
			printf("irqloop %d irqnest %d\n",
			    openpic_irqloop[ci->ci_cpuid],
			    openpic_irqnest[ci->ci_cpuid]);
		}
#endif
		if (openpic_irqloop[ci->ci_cpuid] > 20) {
			DPRINTF("irqloop %d irqnest %d: returning\n",
			    openpic_irqloop[ci->ci_cpuid],
			    openpic_irqnest[ci->ci_cpuid]);
			openpic_irqnest[ci->ci_cpuid]--;
			return;
		}
#ifdef MULTIPROCESSOR
		if (irq == IPI_VECTOR_NOP) {
			ipi_nop[ci->ci_cpuid].ec_count++;
			openpic_eoi(ci->ci_cpuid);
			irq = openpic_read_irq(ci->ci_cpuid);
			continue;
		}
		if (irq == IPI_VECTOR_DDB) {
			ipi_ddb[ci->ci_cpuid].ec_count++;
			openpic_eoi(ci->ci_cpuid);
			openpic_ipi_ddb();
			irq = openpic_read_irq(ci->ci_cpuid);
			continue;
		}
#endif
		iq = &openpic_handler[irq];

		if (iq->iq_ipl <= ci->ci_cpl)
			printf("invalid interrupt %d lvl %d at %d hw %d\n",
			    irq, iq->iq_ipl, ci->ci_cpl,
			    openpic_read(OPENPIC_CPU_PRIORITY(ci->ci_cpuid)));
		if (iq->iq_ipl > maxipl)
			maxipl = iq->iq_ipl;
		openpic_splraise(iq->iq_ipl);
		openpic_eoi(ci->ci_cpuid);

		spurious = 1;
		TAILQ_FOREACH(ih, &iq->iq_list, ih_list) {
			ppc_intr_enable(1);
			KERNEL_LOCK();
			ret = (*ih->ih_fun)(ih->ih_arg);
			if (ret) {
				ih->ih_count.ec_count++;
				spurious = 0;
 			}
			KERNEL_UNLOCK();

			(void)ppc_intr_disable();
			if (intr_shared_edge == 00 && ret == 1)
				break;
 		}
		if (spurious) {
			openpic_spurious.ec_count++;
			DPRINTF("spurious intr %d\n", irq);
		}

		uvmexp.intrs++;
		openpic_setipl(pcpl);

		irq = openpic_read_irq(ci->ci_cpuid);
	}

	openpic_splx(pcpl);	/* Process pendings. */
	openpic_irqnest[ci->ci_cpuid]--;
}

void
openpic_init()
{
	struct cpu_info *ci = curcpu();
	struct intrq *iq;
	int irq;
	u_int x;
	int i;

	openpic_set_priority(ci->ci_cpuid, 15);

	/* disable all interrupts */
	for (irq = 0; irq < openpic_numirq; irq++)
		openpic_write(OPENPIC_SRC_VECTOR(irq), OPENPIC_IMASK);

	for (i = 0; i < openpic_numirq; i++) {
		iq = &openpic_handler[i];
		TAILQ_INIT(&iq->iq_list);
	}

	/* we don't need 8259 pass through mode */
	x = openpic_read(OPENPIC_CONFIG);
	x |= OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
	openpic_write(OPENPIC_CONFIG, x);

	/* initialize all vectors to something sane */
	for (irq = 0; irq < ICU_LEN; irq++) {
		x = irq;
		x |= OPENPIC_IMASK;
		x |= OPENPIC_POLARITY_NEGATIVE;
		x |= OPENPIC_SENSE_LEVEL;
		x |= 8 << OPENPIC_PRIORITY_SHIFT;
		openpic_write(OPENPIC_SRC_VECTOR(irq), x);
	}

	/* send all interrupts to cpu 0 */
	for (irq = 0; irq < openpic_numirq; irq++)
		openpic_write(OPENPIC_IDEST(irq), 1 << 0);

	/* clear all pending interrunts */
	for (irq = 0; irq < ICU_LEN; irq++) {
		openpic_read_irq(ci->ci_cpuid);
		openpic_eoi(ci->ci_cpuid);
	}

#ifdef MULTIPROCESSOR
	/* Set up inter-processor interrupts. */
	/* IPI0 - NOP */
	x = IPI_VECTOR_NOP;
	x |= 15 << OPENPIC_PRIORITY_SHIFT;
	openpic_write(OPENPIC_IPI_VECTOR(0), x);
	/* IPI1 - DDB */
	x = IPI_VECTOR_DDB;
	x |= 15 << OPENPIC_PRIORITY_SHIFT;
	openpic_write(OPENPIC_IPI_VECTOR(1), x);

	/* XXX - ncpus */
	evcount_attach(&ipi_nop[0], "ipi_nop0", &ipi_nopirq);
	evcount_attach(&ipi_nop[1], "ipi_nop1", &ipi_nopirq);
	evcount_attach(&ipi_ddb[0], "ipi_ddb0", &ipi_ddbirq);
	evcount_attach(&ipi_ddb[1], "ipi_ddb1", &ipi_ddbirq);
#endif

	/* clear all pending interrunts */
	for (irq = 0; irq < ICU_LEN; irq++) {
		openpic_read_irq(0);
		openpic_eoi(0);
	}

#if 0
	openpic_write(OPENPIC_SPURIOUS_VECTOR, 255);
#endif

	install_extint(openpic_ext_intr);

	openpic_set_priority(ci->ci_cpuid, 0);
}

#ifdef MULTIPROCESSOR
void
openpic_ipi_ddb()
{
	DPRINTF("ipi_ddb() called\n");
#ifdef DDB
	Debugger();
#endif
}
#endif /* MULTIPROCESSOR */
