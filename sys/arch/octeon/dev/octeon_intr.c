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

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/atomic.h>
#include <machine/intr.h>

#include <octeon/dev/octeonreg.h>
#include <octeon/dev/iobusvar.h>

extern bus_space_handle_t iobus_h;

#define OCTEON_NINTS 64

void	 octeon_intr_makemasks(void);
void	 octeon_splx(int);
uint32_t octeon_iointr(uint32_t, struct trap_frame *);
uint32_t octeon_aux(uint32_t, struct trap_frame *);
int	 octeon_iointr_skip(struct intrhand *, uint64_t, uint64_t);
void	 octeon_setintrmask(int);

struct intrhand *octeon_intrhand[OCTEON_NINTS];

#define	INTPRI_CIU_0	(INTPRI_CLOCK + 1)

uint64_t octeon_intem[MAXCPUS];
uint64_t octeon_imask[MAXCPUS][NIPLS];

void
octeon_intr_init(void)
{
	int cpuid = cpu_number();
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP2_EN0(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN0(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP2_EN1(cpuid), 0);
	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP3_EN1(cpuid), 0);

	set_intr(INTPRI_CIU_0, CR_INT_0, octeon_iointr);
	register_splx_handler(octeon_splx);
}

/*
 * Establish an interrupt handler called from the dispatcher.
 * The interrupt function established should return zero if there was nothing
 * to serve (no int) and non-zero when an interrupt was serviced.
 *
 * Interrupts are numbered from 1 and up where 1 maps to HW int 0.
 * XXX There is no reason to keep this... except for hardcoded interrupts
 * XXX in kernel configuration files...
 */
void *
octeon_intr_establish(int irq, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *ih_what)
{
	int cpuid = cpu_number();
	struct intrhand **p, *q, *ih;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= OCTEON_NINTS || irq < 0)
		panic("intr_establish: illegal irq %d", irq);
#endif

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih_next = NULL;
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, ih_what, (void *)&ih->ih_irq);

	s = splhigh();

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &octeon_intrhand[irq]; (q = *p) != NULL;
	    p = (struct intrhand **)&q->ih_next)
		;
	*p = ih;

	octeon_intem[cpuid] |= 1UL << irq;
	octeon_intr_makemasks();

	splx(s);	/* causes hw mask update */

	return (ih);
}

void
octeon_intr_disestablish(void *ih)
{
	/* XXX */
	panic("%s not implemented", __func__);
}

void
octeon_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	if (CPU_IS_PRIMARY(ci))
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
		for (q = (struct intrhand *)octeon_intrhand[irq]; q != NULL; 
			q = q->ih_next)
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
		uint64_t irqs = 0;
		for (irq = 0; irq < OCTEON_NINTS; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1UL << irq;
		octeon_imask[cpuid][level] = irqs;
	}
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	octeon_imask[cpuid][IPL_NET] |= octeon_imask[cpuid][IPL_BIO];
	octeon_imask[cpuid][IPL_TTY] |= octeon_imask[cpuid][IPL_NET];
	octeon_imask[cpuid][IPL_VM] |= octeon_imask[cpuid][IPL_TTY];
	octeon_imask[cpuid][IPL_CLOCK] |= octeon_imask[cpuid][IPL_VM];
	octeon_imask[cpuid][IPL_HIGH] |= octeon_imask[cpuid][IPL_CLOCK];
	octeon_imask[cpuid][IPL_IPI] |= octeon_imask[cpuid][IPL_HIGH];

	/*
	 * These are pseudo-levels.
	 */
	octeon_imask[cpuid][IPL_NONE] = 0;
}

/*
 * Interrupt dispatcher.
 */
uint32_t
octeon_iointr(uint32_t hwpend, struct trap_frame *frame)
{
	struct cpu_info *ci = curcpu();
	int cpuid = cpu_number();
	uint64_t imr, isr, mask;
	int ipl;
	int bit;
	struct intrhand *ih;
	int rc;
	uint64_t sum0 = CIU_IP2_SUM0(cpuid);
	uint64_t en0 = CIU_IP2_EN0(cpuid);

	isr = bus_space_read_8(&iobus_tag, iobus_h, sum0);
	imr = bus_space_read_8(&iobus_tag, iobus_h, en0);
	bit = 63;

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(&iobus_tag, iobus_h, en0, imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & octeon_imask[cpuid][frame->ipl]) != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process allowed interrupts.
	 */
	if (isr != 0) {
		int lvl, bitno;
		uint64_t tmpisr;

		__asm__ (".set noreorder\n");
		ipl = ci->ci_ipl;
		__asm__ ("sync\n\t.set reorder\n");

		/* Service higher level interrupts first */
		for (lvl = NIPLS - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr & (octeon_imask[cpuid][lvl] ^ octeon_imask[cpuid][lvl - 1]);
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = (struct intrhand *)octeon_intrhand[bitno];
					ih != NULL;
				    ih = ih->ih_next) {
#ifdef MULTIPROCESSOR
					u_int32_t sr;
#endif
					splraise(ih->ih_level);
#ifdef MULTIPROCESSOR
					if (ih->ih_level < IPL_IPI) {
						sr = getsr();
						ENABLEIPI();
						if (ipl < IPL_SCHED)
							__mp_lock(&kernel_lock);
					}
#endif
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						atomic_add_uint64(&ih->ih_count.ec_count, 1);
					}
#ifdef MULTIPROCESSOR
					if (ih->ih_level < IPL_IPI) {
						if (ipl < IPL_SCHED)
							__mp_unlock(&kernel_lock);
						setsr(sr);
					}
#endif
					__asm__ (".set noreorder\n");
					ci->ci_ipl = ipl;
					__asm__ ("sync\n\t.set reorder\n");
				}
				if (rc == 0)
					printf("spurious crime interrupt %d\n", bitno);

				isr ^= mask;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		bus_space_write_8(&iobus_tag, iobus_h, en0, imr);
	}

	return hwpend;
}

void
octeon_setintrmask(int level)
{
	int cpuid = cpu_number();

	bus_space_write_8(&iobus_tag, iobus_h, CIU_IP2_EN0(cpuid),
		octeon_intem[cpuid] & ~octeon_imask[cpuid][level]);
}
