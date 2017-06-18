/*	$OpenBSD: octciu.c,v 1.1 2017/06/18 13:58:44 visa Exp $	*/

/*
 * Copyright (c) 2000-2004 Opsycon AB  (www.opsycon.se)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Driver for OCTEON Central Interrupt Unit (CIU).
 *
 * CIU is present at least on CN3xxx, CN5xxx, CN60xx, CN61xx,
 * CN70xx, and CN71xx.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#include <mips64/mips_cpu.h>

#include <machine/autoconf.h>
#include <machine/fdt.h>
#include <machine/intr.h>
#include <machine/octeonreg.h>

#define OCTCIU_NINTS 128

#define INTPRI_CIU_0	(INTPRI_CLOCK + 1)

struct intrbank {
	uint64_t	en;		/* enable mask register */
	uint64_t	sum;		/* service request register */
	int		id;		/* bank number */
};

#define NBANKS		2
#define BANK_SIZE	64
#define IRQ_TO_BANK(x)	((x) >> 6)
#define IRQ_TO_BIT(x)	((x) & 0x3f)

struct octciu_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	struct intrhand		*sc_intrhand[OCTCIU_NINTS];
	struct intrbank		 sc_ibank[MAXCPUS][NBANKS];
	uint64_t		 sc_intem[MAXCPUS][NBANKS];
	uint64_t		 sc_imask[MAXCPUS][NIPLS][NBANKS];

	int			(*sc_ipi_handler)(void *);

	struct intr_controller	 sc_ic;
};

int	 octciu_match(struct device *, void *, void *);
void	 octciu_attach(struct device *, struct device *, void *);

void	 octciu_init(void);
void	 octciu_intr_makemasks(struct octciu_softc *);
uint32_t octciu_intr(uint32_t, struct trapframe *);
uint32_t octciu_intr_bank(struct octciu_softc *, struct intrbank *,
	    struct trapframe *);
void	*octciu_intr_establish(int, int, int (*)(void *), void *,
	    const char *);
void	*octciu_intr_establish_fdt_idx(void *, int, int, int,
	    int (*)(void *), void *, const char *);
void	 octciu_intr_disestablish(void *);
void	 octciu_splx(int);

uint32_t octciu_ipi_intr(uint32_t, struct trapframe *);
int	 octciu_ipi_establish(int (*)(void *), cpuid_t);
void	 octciu_ipi_set(cpuid_t);
void	 octciu_ipi_clear(cpuid_t);

const struct cfattach octciu_ca = {
	sizeof(struct octciu_softc), octciu_match, octciu_attach
};

struct cfdriver octciu_cd = {
	NULL, "octciu", DV_DULL
};

struct octciu_softc	*octciu_sc;

int
octciu_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "cavium,octeon-3860-ciu");
}

void
octciu_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *fa = aux;
	struct octciu_softc *sc = (struct octciu_softc *)self;

	if (fa->fa_nreg != 1) {
		printf(": expected one IO space, got %d\n", fa->fa_nreg);
		return;
	}

	sc->sc_iot = fa->fa_iot;
	if (bus_space_map(sc->sc_iot, fa->fa_reg[0].addr, fa->fa_reg[0].size,
	    0, &sc->sc_ioh)) {
		printf(": could not map IO space\n");
		return;
	}

	printf("\n");

	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_node = fa->fa_node;
	sc->sc_ic.ic_init = octciu_init;
	sc->sc_ic.ic_establish = octciu_intr_establish;
	sc->sc_ic.ic_establish_fdt_idx = octciu_intr_establish_fdt_idx;
	sc->sc_ic.ic_disestablish = octciu_intr_disestablish;
#ifdef MULTIPROCESSOR
	sc->sc_ic.ic_ipi_establish = octciu_ipi_establish;
	sc->sc_ic.ic_ipi_set = octciu_ipi_set;
	sc->sc_ic.ic_ipi_clear = octciu_ipi_clear;
#endif

	octciu_sc = sc;

	set_intr(INTPRI_CIU_0, CR_INT_0, octciu_intr);
#ifdef MULTIPROCESSOR
	set_intr(INTPRI_IPI, CR_INT_1, octciu_ipi_intr);
#endif

	octciu_init();

	register_splx_handler(octciu_splx);
	octeon_intr_register(&sc->sc_ic);
}

void
octciu_init(void)
{
	struct octciu_softc *sc = octciu_sc;
	int cpuid = cpu_number();

	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP2_EN0(cpuid), 0);
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP3_EN0(cpuid), 0);
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP2_EN1(cpuid), 0);
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP3_EN1(cpuid), 0);

	sc->sc_ibank[cpuid][0].en = CIU_IP2_EN0(cpuid);
	sc->sc_ibank[cpuid][0].sum = CIU_IP2_SUM0(cpuid);
	sc->sc_ibank[cpuid][0].id = 0;
	sc->sc_ibank[cpuid][1].en = CIU_IP2_EN1(cpuid);
	sc->sc_ibank[cpuid][1].sum = CIU_INT32_SUM1;
	sc->sc_ibank[cpuid][1].id = 1;
}

void *
octciu_intr_establish(int irq, int level, int (*ih_fun)(void *),
    void *ih_arg, const char *ih_what)
{
	struct octciu_softc *sc = octciu_sc;
	struct intrhand **p, *q, *ih;
	int cpuid = cpu_number();
	int flags;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= OCTCIU_NINTS || irq < 0)
		panic("intr_establish: illegal irq %d", irq);
#endif

	flags = (level & IPL_MPSAFE) ? IH_MPSAFE : 0;
	level &= ~IPL_MPSAFE;

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih_next = NULL;
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_level = level;
	ih->ih_flags = flags;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, ih_what, &ih->ih_irq);

	s = splhigh();

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &sc->sc_intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		continue;
	*p = ih;

	sc->sc_intem[cpuid][IRQ_TO_BANK(irq)] |= 1UL << IRQ_TO_BIT(irq);
	octciu_intr_makemasks(sc);

	splx(s);	/* causes hw mask update */

	return (ih);
}

void *
octciu_intr_establish_fdt_idx(void *cookie, int node, int idx, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *ih_what)
{
	uint32_t *cells;
	int irq, len;

	len = OF_getproplen(node, "interrupts");
	if (len / (sizeof(uint32_t) * 2) <= idx ||
	    len % (sizeof(uint32_t) * 2) != 0)
		return NULL;

	cells = malloc(len, M_TEMP, M_NOWAIT);
	if (cells == NULL)
		return NULL;

	OF_getpropintarray(node, "interrupts", cells, len);
	irq = cells[idx * 2] * BANK_SIZE + cells[idx * 2 + 1];

	free(cells, M_TEMP, len);

	return octciu_intr_establish(irq, level, ih_fun, ih_arg, ih_what);
}

void
octciu_intr_disestablish(void *_ih)
{
	struct intrhand *ih = _ih;
	struct intrhand *p;
	struct octciu_softc *sc = octciu_sc;
	unsigned int irq = ih->ih_irq;
	int cpuid = cpu_number();
	int s;

	KASSERT(irq < OCTCIU_NINTS);

	s = splhigh();

	if (ih == sc->sc_intrhand[irq]) {
		sc->sc_intrhand[irq] = ih->ih_next;

		if (sc->sc_intrhand[irq] == NULL)
			sc->sc_intem[cpuid][IRQ_TO_BANK(irq)] &=
			    ~(1UL << IRQ_TO_BIT(irq));
	} else {
		for (p = sc->sc_intrhand[irq]; p != NULL; p = p->ih_next) {
			if (p->ih_next == ih) {
				p->ih_next = ih->ih_next;
				break;
			}
		}
		if (p == NULL)
			panic("%s: intrhand %p has not been registered",
			    __func__, ih);
	}
	free(ih, M_DEVBUF, sizeof(*ih));

	octciu_intr_makemasks(sc);
	splx(s);	/* causes hw mask update */
}

/*
 * Recompute interrupt masks.
 */
void
octciu_intr_makemasks(struct octciu_softc *sc)
{
	int cpuid = cpu_number();
	int irq, level;
	struct intrhand *q;
	uint intrlevel[OCTCIU_NINTS];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < OCTCIU_NINTS; irq++) {
		uint levels = 0;
		for (q = sc->sc_intrhand[irq]; q != NULL; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/*
	 * Then figure out which IRQs use each level.
	 * Note that we make sure never to overwrite imask[IPL_HIGH], in
	 * case an interrupt occurs during intr_disestablish() and causes
	 * an unfortunate splx() while we are here recomputing the masks.
	 */
	for (level = IPL_NONE; level < NIPLS; level++) {
		uint64_t mask[NBANKS] = {};
		for (irq = 0; irq < OCTCIU_NINTS; irq++)
			if (intrlevel[irq] & (1 << level))
				mask[IRQ_TO_BANK(irq)] |=
				    1UL << IRQ_TO_BIT(irq);
		sc->sc_imask[cpuid][level][0] = mask[0];
		sc->sc_imask[cpuid][level][1] = mask[1];
	}
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
#define ADD_MASK(dst, src) do {	\
	dst[0] |= src[0];	\
	dst[1] |= src[1];	\
} while (0)
	ADD_MASK(sc->sc_imask[cpuid][IPL_NET], sc->sc_imask[cpuid][IPL_BIO]);
	ADD_MASK(sc->sc_imask[cpuid][IPL_TTY], sc->sc_imask[cpuid][IPL_NET]);
	ADD_MASK(sc->sc_imask[cpuid][IPL_VM], sc->sc_imask[cpuid][IPL_TTY]);
	ADD_MASK(sc->sc_imask[cpuid][IPL_CLOCK], sc->sc_imask[cpuid][IPL_VM]);
	ADD_MASK(sc->sc_imask[cpuid][IPL_HIGH], sc->sc_imask[cpuid][IPL_CLOCK]);
	ADD_MASK(sc->sc_imask[cpuid][IPL_IPI], sc->sc_imask[cpuid][IPL_HIGH]);

	/*
	 * These are pseudo-levels.
	 */
	sc->sc_imask[cpuid][IPL_NONE][0] = 0;
	sc->sc_imask[cpuid][IPL_NONE][1] = 0;
}

static inline int
octciu_next_irq(uint64_t *isr)
{
	uint64_t irq, tmp = *isr;

	if (tmp == 0)
		return -1;

	asm volatile (
	"	.set push\n"
	"	.set mips64\n"
	"	dclz	%0, %0\n"
	"	.set pop\n"
	: "=r" (tmp) : "0" (tmp));

	irq = 63u - tmp;
	*isr &= ~(1u << irq);
	return irq;
}

/*
 * Dispatch interrupts in given bank.
 */
uint32_t
octciu_intr_bank(struct octciu_softc *sc, struct intrbank *bank,
    struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct intrhand *ih;
	uint64_t imr, isr, mask;
	int handled, ipl, irq;
#ifdef MULTIPROCESSOR
	register_t sr;
	int need_lock;
#endif

	isr = bus_space_read_8(sc->sc_iot, sc->sc_ioh, bank->sum);
	imr = bus_space_read_8(sc->sc_iot, sc->sc_ioh, bank->en);

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, bank->en, imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & sc->sc_imask[ci->ci_cpuid][frame->ipl][bank->id])
	    != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}
	if (isr == 0)
		return 1;

	/*
	 * Now process allowed interrupts.
	 */

	__asm__ (".set noreorder\n");
	ipl = ci->ci_ipl;
	mips_sync();
	__asm__ (".set reorder\n");

	while ((irq = octciu_next_irq(&isr)) >= 0) {
		irq += bank->id * BANK_SIZE;
		handled = 0;
		for (ih = sc->sc_intrhand[irq]; ih != NULL; ih = ih->ih_next) {
			splraise(ih->ih_level);
#ifdef MULTIPROCESSOR
			if (ih->ih_level < IPL_IPI) {
				sr = getsr();
				ENABLEIPI();
			}
			if (ih->ih_flags & IH_MPSAFE)
				need_lock = 0;
			else
				need_lock = ih->ih_level < IPL_CLOCK;
			if (need_lock)
				__mp_lock(&kernel_lock);
#endif
			if ((*ih->ih_fun)(ih->ih_arg) != 0) {
				handled = 1;
				atomic_inc_long(
				    (unsigned long *)&ih->ih_count.ec_count);
			}
#ifdef MULTIPROCESSOR
			if (need_lock)
				__mp_unlock(&kernel_lock);
			if (ih->ih_level < IPL_IPI)
				setsr(sr);
#endif
		}
		if (!handled)
			printf("spurious interrupt %d\n", irq);
	}

	__asm__ (".set noreorder\n");
	ci->ci_ipl = ipl;
	mips_sync();
	__asm__ (".set reorder\n");

	/*
	 * Reenable interrupts which have been serviced.
	 */
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, bank->en, imr);

	return 1;
}

uint32_t
octciu_intr(uint32_t hwpend, struct trapframe *frame)
{
	struct octciu_softc *sc = octciu_sc;
	int cpuid = cpu_number();
	int handled;

	handled = octciu_intr_bank(sc, &sc->sc_ibank[cpuid][0], frame);
	handled |= octciu_intr_bank(sc, &sc->sc_ibank[cpuid][1], frame);
	return handled ? hwpend : 0;
}

void
octciu_splx(int newipl)
{
	struct octciu_softc *sc = octciu_sc;
	struct cpu_info *ci = curcpu();
	cpuid_t cpuid = ci->ci_cpuid;

	/* Update IPL. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	mips_sync();
	__asm__ (".set reorder\n");

	/* Set hardware masks. */
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, sc->sc_ibank[cpuid][0].en,
	    sc->sc_intem[cpuid][0] & ~sc->sc_imask[cpuid][newipl][0]);
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, sc->sc_ibank[cpuid][1].en,
	    sc->sc_intem[cpuid][1] & ~sc->sc_imask[cpuid][newipl][1]);

	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

#ifdef MULTIPROCESSOR
uint32_t
octciu_ipi_intr(uint32_t hwpend, struct trapframe *frame)
{
	struct octciu_softc *sc = octciu_sc;
	u_long cpuid = cpu_number();

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP3_EN0(cpuid), 0);

	if (sc->sc_ipi_handler == NULL)
		return hwpend;

	sc->sc_ipi_handler((void *)cpuid);

	/*
	 * Reenable interrupts which have been serviced.
	 */
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP3_EN0(cpuid),
		(1ULL << CIU_INT_MBOX0)|(1ULL << CIU_INT_MBOX1));
	return hwpend;
}

int
octciu_ipi_establish(int (*func)(void *), cpuid_t cpuid)
{
	struct octciu_softc *sc = octciu_sc;

	if (cpuid == 0)
		sc->sc_ipi_handler = func;

	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_MBOX_CLR(cpuid),
		0xffffffff);
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_IP3_EN0(cpuid),
		(1ULL << CIU_INT_MBOX0)|(1ULL << CIU_INT_MBOX1));

	return 0;
}

void
octciu_ipi_set(cpuid_t cpuid)
{
	struct octciu_softc *sc = octciu_sc;

	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_MBOX_SET(cpuid), 1);
}

void
octciu_ipi_clear(cpuid_t cpuid)
{
	struct octciu_softc *sc = octciu_sc;
	uint64_t clr;

	clr = bus_space_read_8(sc->sc_iot, sc->sc_ioh, CIU_MBOX_CLR(cpuid));
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, CIU_MBOX_CLR(cpuid), clr);
}
#endif /* MULTIPROCESSOR */
