/*	$OpenBSD: footbridge_irqhandler.c,v 1.4 2004/08/17 19:40:45 drahn Exp $	*/
/*	$NetBSD: footbridge_irqhandler.c,v 1.9 2003/06/16 20:00:57 thorpej Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARM_SPL_NOINLINE
#define	ARM_SPL_NOINLINE
#endif

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>

#include <machine/intr.h>
#include <machine/cpu.h>
#include <arm/footbridge/dc21285mem.h>
#include <arm/footbridge/dc21285reg.h>

#include <dev/pci/pcivar.h>

#include "isa.h"
#if NISA > 0
#include <dev/isa/isavar.h>
#endif

/* Interrupt handler queues. */
static struct intrq footbridge_intrq[NIRQ];

/* Interrupts to mask at each level. */
int footbridge_imask[NIPL];

/* Software copy of the IRQs we have enabled. */
__volatile uint32_t intr_enabled;

/* Current interrupt priority level */
__volatile int current_spl_level;

/* Interrupts pending */
__volatile int footbridge_ipending;

void footbridge_intr_dispatch(struct clockframe *frame);

void footbridge_do_pending(void);

static const uint32_t si_to_irqbit[SI_NQUEUES] =
	{ IRQ_SOFTINT,
	  IRQ_RESERVED0,
	  IRQ_RESERVED1,
	  IRQ_RESERVED2 };

#define	SI_TO_IRQBIT(si)	(1U << si_to_irqbit[(si)])

/*
 * Map a software interrupt queue to an interrupt priority level.
 */
static const int si_to_ipl[SI_NQUEUES] = {
	IPL_SOFT,		/* SI_SOFT */
	IPL_SOFTCLOCK,		/* SI_SOFTCLOCK */
	IPL_SOFTNET,		/* SI_SOFTNET */
	IPL_SOFTSERIAL,		/* SI_SOFTSERIAL */
};
static __inline void
footbridge_enable_irq(int irq)
{
	intr_enabled |= (1U << irq);
	
	footbridge_set_intrmask();
}

static __inline void
footbridge_disable_irq(int irq)
{
	intr_enabled &= ~(1U << irq);
	footbridge_set_intrmask();
}

/*
 * NOTE: This routine must be called with interrupts disabled in the CPSR.
 */
static void
footbridge_intr_calculate_masks(void)
{
	struct intrq *iq;
	struct intrhand *ih;
	int irq, ipl;

	/* First, figure out which IPLs each IRQ has. */
	for (irq = 0; irq < NIRQ; irq++) {
		int levels = 0;
		iq = &footbridge_intrq[irq];
		footbridge_disable_irq(irq);
		for (ih = TAILQ_FIRST(&iq->iq_list); ih != NULL;
		     ih = TAILQ_NEXT(ih, ih_list))
			levels |= (1U << ih->ih_ipl);
		iq->iq_levels = levels;
	}

	/* Next, figure out which IRQs are used by each IPL. */
	for (ipl = 0; ipl < NIPL; ipl++) {
		int irqs = 0;
		for (irq = 0; irq < NIRQ; irq++) {
			if (footbridge_intrq[irq].iq_levels & (1U << ipl))
				irqs |= (1U << irq);
		}
		footbridge_imask[ipl] = irqs;
	}

	/* IPL_NONE must open up all interrupts */
	footbridge_imask[IPL_NONE] = 0;

	/*
	 * Initialize the soft interrupt masks to block themselves.
	 */
	footbridge_imask[IPL_SOFT] = SI_TO_IRQBIT(SI_SOFT);
	footbridge_imask[IPL_SOFTCLOCK] = SI_TO_IRQBIT(SI_SOFTCLOCK);
	footbridge_imask[IPL_SOFTNET] = SI_TO_IRQBIT(SI_SOFTNET);
	footbridge_imask[IPL_SOFTSERIAL] = SI_TO_IRQBIT(SI_SOFTSERIAL);

	footbridge_imask[IPL_SOFTCLOCK] |= footbridge_imask[IPL_SOFT];
	footbridge_imask[IPL_SOFTNET] |= footbridge_imask[IPL_SOFTCLOCK];

	/*
	 * Enforce a heirarchy that gives "slow" device (or devices with
	 * limited input buffer space/"real-time" requirements) a better
	 * chance at not dropping data.
	 */
	footbridge_imask[IPL_BIO] |= footbridge_imask[IPL_SOFTNET];
	footbridge_imask[IPL_NET] |= footbridge_imask[IPL_BIO];
	footbridge_imask[IPL_SOFTSERIAL] |= footbridge_imask[IPL_NET];

	footbridge_imask[IPL_TTY] |= footbridge_imask[IPL_SOFTSERIAL];

	/*
	 * splvm() blocks all interrupts that use the kernel memory
	 * allocation facilities.
	 */
	footbridge_imask[IPL_VM] |= footbridge_imask[IPL_TTY];

	/*
	 * Audio devices are not allowed to perform memory allocation
	 * in their interrupt routines, and they have fairly "real-time"
	 * requirements, so give them a high interrupt priority.
	 */
	footbridge_imask[IPL_AUDIO] |= footbridge_imask[IPL_VM];

	/*
	 * splclock() must block anything that uses the scheduler.
	 */
	footbridge_imask[IPL_CLOCK] |= footbridge_imask[IPL_AUDIO];

	/*
	 * footbridge has seperate statclock.
	 */
	footbridge_imask[IPL_STATCLOCK] |= footbridge_imask[IPL_CLOCK];

	/*
	 * splhigh() must block "everything".
	 */
	footbridge_imask[IPL_HIGH] |= footbridge_imask[IPL_STATCLOCK];

	/*
	 * XXX We need serial drivers to run at the absolute highest priority
	 * in order to avoid overruns, so serial > high.
	 */
	footbridge_imask[IPL_SERIAL] |= footbridge_imask[IPL_HIGH];

	/*
	 * Calculate the ipl level to go to when handling this interrupt
	 */
	for (irq = 0; irq < NIRQ; irq++) {
		int irqs = (1U << irq);
		iq = &footbridge_intrq[irq];
		if (TAILQ_FIRST(&iq->iq_list) != NULL)
			footbridge_enable_irq(irq);
		for (ih = TAILQ_FIRST(&iq->iq_list); ih != NULL;
		     ih = TAILQ_NEXT(ih, ih_list))
			irqs |= footbridge_imask[ih->ih_ipl];
		iq->iq_mask = irqs;
	}
}

int
_splraise(int ipl)
{
    return (footbridge_splraise(ipl));
}

/* this will always take us to the ipl passed in */
void
splx(int new)
{
    footbridge_splx(new);
}

int
_spllower(int ipl)
{
    return (footbridge_spllower(ipl));
}

__inline void
footbridge_do_pending(void)
{
#if 0
	static __cpu_simple_lock_t processing = __SIMPLELOCK_UNLOCKED;
#else
	static int processing;
#endif
	uint32_t new, oldirqstate;

#if 0
	if (__cpu_simple_lock_try(&processing) == 0)
		return;
#else
	if (processing)
		return;
	processing = 1;
#endif

	new = current_spl_level;
	
	oldirqstate = disable_interrupts(I32_bit);

#define	DO_SOFTINT(si)							\
	if ((footbridge_ipending & ~new) & SI_TO_IRQBIT(si)) {		\
		footbridge_ipending &= ~SI_TO_IRQBIT(si);		\
		current_spl_level |= footbridge_imask[si_to_ipl[(si)]];	\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
		current_spl_level = new;				\
	}
	DO_SOFTINT(SI_SOFTSERIAL);
	DO_SOFTINT(SI_SOFTNET);
	DO_SOFTINT(SI_SOFTCLOCK);
	DO_SOFTINT(SI_SOFT);
	
#if 0
	__cpu_simple_unlock(&processing);
#else
	processing = 0;
#endif

	restore_interrupts(oldirqstate);
}


/* called from splhigh, so the matching splx will set the interrupt up.*/
void
_setsoftintr(int si)
{
	int oldirqstate;

	oldirqstate = disable_interrupts(I32_bit);
	footbridge_ipending |= SI_TO_IRQBIT(si);
	restore_interrupts(oldirqstate);

	/* Process unmasked pending soft interrupts. */
	if ((footbridge_ipending & INT_SWMASK) & ~current_spl_level)
		footbridge_do_pending();
}

void
footbridge_intr_init(void)
{
	struct intrq *iq;
	int i;

	intr_enabled = 0;
	current_spl_level = 0xffffffff;
	footbridge_ipending = 0;
	footbridge_set_intrmask();
	
	for (i = 0; i < NIRQ; i++) {
		iq = &footbridge_intrq[i];
		TAILQ_INIT(&iq->iq_list);
	}
	
	footbridge_intr_calculate_masks();

	/* Enable IRQ's, we don't have any FIQ's*/
	enable_interrupts(I32_bit);
}

void *
footbridge_intr_claim(int irq, int ipl, char *name, int (*func)(void *), void *arg)
{
	struct intrq *iq;
	struct intrhand *ih;
	u_int oldirqstate;

	if (irq < 0 || irq > NIRQ)
		panic("footbridge_intr_establish: IRQ %d out of range", irq);

	ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
	{
		printf("No memory");
		return (NULL);
	}
		
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	ih->ih_irq = irq;

	iq = &footbridge_intrq[irq];

	iq->iq_ist = IST_LEVEL;

	oldirqstate = disable_interrupts(I32_bit);

	TAILQ_INSERT_TAIL(&iq->iq_list, ih, ih_list);

	footbridge_intr_calculate_masks();

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, (void *)&ih->ih_irq,
		    &evcount_intr);
	
	restore_interrupts(oldirqstate);
	
	return(ih);
}

void
footbridge_intr_disestablish(void *cookie)
{
	struct intrhand *ih = cookie;
	struct intrq *iq = &footbridge_intrq[ih->ih_irq];
	int oldirqstate;

	/* XXX need to free ih ? */
	oldirqstate = disable_interrupts(I32_bit);

	TAILQ_REMOVE(&iq->iq_list, ih, ih_list);

	footbridge_intr_calculate_masks();

	restore_interrupts(oldirqstate);
}

static uint32_t footbridge_intstatus(void);

static inline uint32_t footbridge_intstatus()
{
    return ((__volatile uint32_t*)(DC21285_ARMCSR_VBASE))[IRQ_STATUS>>2];
}

/* called with external interrupts disabled */
void
footbridge_intr_dispatch(struct clockframe *frame)
{
	struct intrq *iq;
	struct intrhand *ih;
	int oldirqstate, pcpl, irq, ibit, hwpend;

	pcpl = current_spl_level;

	hwpend = footbridge_intstatus();

	/*
	 * Disable all the interrupts that are pending.  We will
	 * reenable them once they are processed and not masked.
	 */
	intr_enabled &= ~hwpend;
	footbridge_set_intrmask();

	while (hwpend != 0) {
		int intr_rc = 0;
		irq = ffs(hwpend) - 1;
		ibit = (1U << irq);

		hwpend &= ~ibit;

		if (pcpl & ibit) {
			/*
			 * IRQ is masked; mark it as pending and check
			 * the next one.  Note: the IRQ is already disabled.
			 */
			footbridge_ipending |= ibit;
			continue;
		}

		footbridge_ipending &= ~ibit;

		iq = &footbridge_intrq[irq];
		uvmexp.intrs++;
		current_spl_level |= iq->iq_mask;
		oldirqstate = enable_interrupts(I32_bit);
		for (ih = TAILQ_FIRST(&iq->iq_list);
			((ih != NULL) && (intr_rc != 1));
		     ih = TAILQ_NEXT(ih, ih_list)) {
			intr_rc = (*ih->ih_func)(ih->ih_arg ?
			    ih->ih_arg : frame);
			if (intr_rc)
				ih->ih_count.ec_count++;

		}
		restore_interrupts(oldirqstate);

		current_spl_level = pcpl;

		/* Re-enable this interrupt now that's it's cleared. */
		intr_enabled |= ibit;
		footbridge_set_intrmask();

		/* also check for any new interrupts that may have occured,
		 * that we can handle at this spl level */
		hwpend |= (footbridge_ipending & ICU_INT_HWMASK) & ~pcpl;
	}

	/* Check for pendings soft intrs. */
        if ((footbridge_ipending & INT_SWMASK) & ~current_spl_level) {
	    /* 
	     * XXX this feels the wrong place to enable irqs, as some
	     * soft ints are higher priority than hardware irqs
	     */
                oldirqstate = enable_interrupts(I32_bit);
                footbridge_do_pending();
                restore_interrupts(oldirqstate);
        }
}
