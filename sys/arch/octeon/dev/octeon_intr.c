/*	$OpenBSD: octeon_intr.c,v 1.18 2016/12/04 11:52:51 visa Exp $	*/

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
 * Interrupt support for Octeon Processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <mips64/mips_cpu.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/octeonreg.h>

#include <octeon/dev/iobusvar.h>

extern bus_space_handle_t iobus_h;

#define OCTEON_NINTS 128

struct intrbank {
	uint64_t	en;		/* enable mask register */
	uint64_t	sum;		/* service request register */
	int		id;		/* bank number */
};

#define NBANKS		2
#define BANK_SIZE	64
#define IRQ_TO_BANK(x)	((x) >> 6)
#define IRQ_TO_BIT(x)	((x) & 0x3f)

void	 octeon_intr_makemasks(void);
void	 octeon_splx(int);
uint32_t octeon_iointr_bank(struct trapframe *, struct intrbank *);
uint32_t octeon_iointr(uint32_t, struct trapframe *);
void	 octeon_setintrmask(int);

struct intrhand *octeon_intrhand[OCTEON_NINTS];

#define	INTPRI_CIU_0	(INTPRI_CLOCK + 1)

uint64_t octeon_intem[MAXCPUS][NBANKS];
uint64_t octeon_imask[MAXCPUS][NIPLS][NBANKS];
struct intrbank octeon_ibank[MAXCPUS][NBANKS];

void
octeon_intr_init(void)
{
	int cpuid = cpu_number();
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP2_EN0(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN0(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP2_EN1(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN1(cpuid), 0);

	octeon_ibank[cpuid][0].en = CIU_IP2_EN0(cpuid);
	octeon_ibank[cpuid][0].sum = CIU_IP2_SUM0(cpuid);
	octeon_ibank[cpuid][0].id = 0;
	octeon_ibank[cpuid][1].en = CIU_IP2_EN1(cpuid);
	octeon_ibank[cpuid][1].sum = CIU_INT32_SUM1;
	octeon_ibank[cpuid][1].id = 1;

	set_intr(INTPRI_CIU_0, CR_INT_0, octeon_iointr);
	register_splx_handler(octeon_splx);
}

/*
 * Establish an interrupt handler called from the dispatcher.
 * The interrupt function established should return zero if there was nothing
 * to serve (no int) and non-zero when an interrupt was serviced.
 */
void *
octeon_intr_establish(int irq, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *ih_what)
{
	int cpuid = cpu_number();
	struct intrhand **p, *q, *ih;
	int flags;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= OCTEON_NINTS || irq < 0)
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
	for (p = &octeon_intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		continue;
	*p = ih;

	octeon_intem[cpuid][IRQ_TO_BANK(irq)] |= 1UL << IRQ_TO_BIT(irq);
	octeon_intr_makemasks();

	splx(s);	/* causes hw mask update */

	return (ih);
}

void
octeon_intr_disestablish(void *_ih)
{
	struct intrhand *ih = _ih;
	struct intrhand *p;
	unsigned int irq = ih->ih_irq;
	int cpuid = cpu_number();
	int s;

	KASSERT(irq < OCTEON_NINTS);

	s = splhigh();

	if (ih == octeon_intrhand[irq]) {
		octeon_intrhand[irq] = ih->ih_next;

		if (octeon_intrhand[irq] == NULL)
			octeon_intem[cpuid][IRQ_TO_BANK(irq)] &=
			    ~(1UL << IRQ_TO_BIT(irq));
	} else {
		for (p = octeon_intrhand[irq]; p != NULL; p = p->ih_next) {
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

	octeon_intr_makemasks();
	splx(s);	/* causes hw mask update */
}

void
octeon_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	mips_sync();
	__asm__ (".set reorder\n");
	octeon_setintrmask(newipl);

	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Recompute interrupt masks.
 */
void
octeon_intr_makemasks()
{
	int cpuid = cpu_number();
	int irq, level;
	struct intrhand *q;
	uint intrlevel[OCTEON_NINTS];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < OCTEON_NINTS; irq++) {
		uint levels = 0;
		for (q = octeon_intrhand[irq]; q != NULL; q = q->ih_next)
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
		for (irq = 0; irq < OCTEON_NINTS; irq++)
			if (intrlevel[irq] & (1 << level))
				mask[IRQ_TO_BANK(irq)] |=
				    1UL << IRQ_TO_BIT(irq);
		octeon_imask[cpuid][level][0] = mask[0];
		octeon_imask[cpuid][level][1] = mask[1];
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
	ADD_MASK(octeon_imask[cpuid][IPL_NET], octeon_imask[cpuid][IPL_BIO]);
	ADD_MASK(octeon_imask[cpuid][IPL_TTY], octeon_imask[cpuid][IPL_NET]);
	ADD_MASK(octeon_imask[cpuid][IPL_VM], octeon_imask[cpuid][IPL_TTY]);
	ADD_MASK(octeon_imask[cpuid][IPL_CLOCK], octeon_imask[cpuid][IPL_VM]);
	ADD_MASK(octeon_imask[cpuid][IPL_HIGH], octeon_imask[cpuid][IPL_CLOCK]);
	ADD_MASK(octeon_imask[cpuid][IPL_IPI], octeon_imask[cpuid][IPL_HIGH]);

	/*
	 * These are pseudo-levels.
	 */
	octeon_imask[cpuid][IPL_NONE][0] = 0;
	octeon_imask[cpuid][IPL_NONE][1] = 0;
}

static inline int
octeon_next_irq(uint64_t *isr)
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
octeon_iointr_bank(struct trapframe *frame, struct intrbank *bank)
{
	struct cpu_info *ci = curcpu();
	struct intrhand *ih;
	uint64_t imr, isr, mask;
	int handled, ipl, irq;
#ifdef MULTIPROCESSOR
	register_t sr;
	int need_lock;
#endif

	isr = bus_space_read_8(&iobus_tag, iobus_h, bank->sum);
	imr = bus_space_read_8(&iobus_tag, iobus_h, bank->en);

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(&iobus_tag, iobus_h, bank->en, imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & octeon_imask[ci->ci_cpuid][frame->ipl][bank->id])
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

	while ((irq = octeon_next_irq(&isr)) >= 0) {
		irq += bank->id * BANK_SIZE;
		handled = 0;
		for (ih = octeon_intrhand[irq]; ih != NULL; ih = ih->ih_next) {
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
	bus_space_write_8(&iobus_tag, iobus_h, bank->en, imr);

	return 1;
}

/*
 * Interrupt dispatcher.
 */
uint32_t
octeon_iointr(uint32_t hwpend, struct trapframe *frame)
{
	int cpuid = cpu_number();
	int handled;

	handled = octeon_iointr_bank(frame, &octeon_ibank[cpuid][0]);
	handled |= octeon_iointr_bank(frame, &octeon_ibank[cpuid][1]);
	return handled ? hwpend : 0;
}

void
octeon_setintrmask(int level)
{
	int cpuid = cpu_number();

	bus_space_write_8(&iobus_tag, iobus_h, octeon_ibank[cpuid][0].en,
	    octeon_intem[cpuid][0] & ~octeon_imask[cpuid][level][0]);
	bus_space_write_8(&iobus_tag, iobus_h, octeon_ibank[cpuid][1].en,
	    octeon_intem[cpuid][1] & ~octeon_imask[cpuid][level][1]);
}
