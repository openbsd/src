/*	$OpenBSD: pxa2x0_intr.c,v 1.7 2005/01/17 04:27:20 drahn Exp $ */
/*	$NetBSD: pxa2x0_intr.c,v 1.5 2003/07/15 00:24:55 lukem Exp $	*/

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IRQ handler for the Intel PXA2X0 processor.
 * It has integrated interrupt controller.
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: pxa2x0_intr.c,v 1.5 2003/07/15 00:24:55 lukem Exp $");
*/

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/evcount.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/lock.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_intr.h>
#include <arm/sa11x0/sa11x0_var.h>

/*
 * INTC autoconf glue
 */
int	pxaintc_match(struct device *, void *, void *);
void	pxaintc_attach(struct device *, struct device *, void *);

#ifdef __NetBSD__
CFATTACH_DECL(pxaintc, sizeof(struct device),
    pxaintc_match, pxaintc_attach, NULL, NULL);
#else
struct cfattach pxaintc_ca = {
        sizeof(struct device), pxaintc_match, pxaintc_attach
};

struct cfdriver pxaintc_cd = {
	NULL, "pxaintc", DV_DULL
};

#endif

static int pxaintc_attached;

int pxa2x0_stray_interrupt(void *);
void pxa2x0_init_interrupt_masks(void);

/*
 * interrupt dispatch table. 
 */
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
};
#endif

static struct intrhandler{
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	TAILQ_HEAD(,intrhand) list;
#else
	pxa2x0_irq_handler_t func;
#endif
	void *arg;		/* NULL for stackframe */
	/* struct evbnt ev; */
	char *name;
	int ih_irq;
	struct evcount ih_count;
}
handler[ICU_LEN];

__volatile int softint_pending;
__volatile int current_spl_level;
__volatile int intr_mask;
/* interrupt masks for each level */
int pxa2x0_imask[NIPL];
static int extirq_level[ICU_LEN];


int
pxaintc_match(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;

	if (pxaintc_attached || pxa->pxa_addr != PXA2X0_INTCTL_BASE)
		return (0);

	return (1);
}

void
pxaintc_attach(struct device *parent, struct device *self, void *args)
{
	int i;

	pxaintc_attached = 1;

	printf(": Interrupt Controller\n");

#define	SAIPIC_ICCR	0x14

	write_icu(SAIPIC_ICCR, 1);
	write_icu(SAIPIC_MR, 0);

	for(i = 0; i < sizeof handler / sizeof handler[0]; ++i){
		handler[i].name = "stray";
		handler[i].func = pxa2x0_stray_interrupt;
		handler[i].arg = (void *)(u_int32_t) i;

		extirq_level[i] = IPL_SERIAL;
	}

	pxa2x0_init_interrupt_masks();

	_splraise(IPL_SERIAL);
	enable_interrupts(I32_bit);
}

/*
 * Invoked very early on from the board-specific initarm(), in order to
 * inform us the virtual address of the interrupt controller's registers.
 */
vaddr_t pxaic_base;	
void
pxa2x0_intr_bootstrap(vaddr_t addr)
{

	pxaic_base = addr;
}


/*
 * Map a software interrupt queue to an interrupt priority level.
 */
static const int si_to_ipl[SI_NQUEUES] = {
	IPL_SOFT,		/* SI_SOFT */
	IPL_SOFTCLOCK,		/* SI_SOFTCLOCK */
	IPL_SOFTNET,		/* SI_SOFTNET */
	IPL_SOFTSERIAL,		/* SI_SOFTSERIAL */
};

/*
 * called from irq_entry.
 */
void
pxa2x0_irq_handler(void *arg)
{
	struct clockframe *frame = arg;
	uint32_t irqbits;
	int irqno;
	int saved_spl_level;

	saved_spl_level = current_spl_level;

	/* get pending IRQs */
	irqbits = read_icu(SAIPIC_IP);

	while ((irqno = find_first_bit(irqbits)) >= 0) {
		/* XXX: Shuould we handle IRQs in priority order? */

		/* raise spl to stop interrupts of lower priorities */
		if (saved_spl_level < extirq_level[irqno])
			pxa2x0_setipl(extirq_level[irqno]);

#ifdef notyet
		/* Enable interrupt */
#endif
#ifndef MULTIPLE_HANDLERS_ON_ONE_IRQ
		(* handler[irqno].func)( 
			handler[irqno].arg == 0
			? frame : handler[irqno].arg );
		handler[irqno].ih_count.ec_count++;
#else
		/* process all handlers for this interrupt.
		   XXX not yet */
#endif
		
#ifdef notyet
		/* Disable interrupt */
#endif

		irqbits &= ~(1<<irqno);
	}

	/* restore spl to that was when this interrupt happen */
	pxa2x0_setipl(saved_spl_level);
			
	if(softint_pending & intr_mask)
		pxa2x0_do_pending();
}

int
pxa2x0_stray_interrupt(void *cookie)
{
	int irqno = (int)cookie;
	printf("stray interrupt %d\n", irqno);

	if (PXA2X0_IRQ_MIN <= irqno && irqno < ICU_LEN){
		int save = disable_interrupts(I32_bit);
		write_icu(SAIPIC_MR,
		    read_icu(SAIPIC_MR) & ~(1U<<irqno));
		restore_interrupts(save);
	}

	return 0;
}



/*
 * Interrupt Mask Handling
 */

void
pxa2x0_update_intr_masks(int irqno, int level)
{
	int mask = 1U<<irqno;
	int psw = disable_interrupts(I32_bit);
	int i;

	for(i = 0; i < level; ++i)
		pxa2x0_imask[i] |= mask; /* Enable interrupt at lower level */

	for( ; i < NIPL-1; ++i)
		pxa2x0_imask[i] &= ~mask; /* Disable itnerrupt at upper level */

	/*
	 * Enforce a heirarchy that gives "slow" device (or devices with
	 * limited input buffer space/"real-time" requirements) a better
	 * chance at not dropping data.
	 */
	pxa2x0_imask[IPL_BIO] &= pxa2x0_imask[IPL_SOFTNET];
	pxa2x0_imask[IPL_NET] &= pxa2x0_imask[IPL_BIO];
	pxa2x0_imask[IPL_SOFTSERIAL] &= pxa2x0_imask[IPL_NET];
	pxa2x0_imask[IPL_TTY] &= pxa2x0_imask[IPL_SOFTSERIAL];

	/*
	 * splvm() blocks all interrupts that use the kernel memory
	 * allocation facilities.
	 */
	pxa2x0_imask[IPL_VM] &= pxa2x0_imask[IPL_TTY];

	/*
	 * Audio devices are not allowed to perform memory allocation
	 * in their interrupt routines, and they have fairly "real-time"
	 * requirements, so give them a high interrupt priority.
	 */
	pxa2x0_imask[IPL_AUDIO] &= pxa2x0_imask[IPL_VM];

	/*
	 * splclock() must block anything that uses the scheduler.
	 */
	pxa2x0_imask[IPL_CLOCK] &= pxa2x0_imask[IPL_AUDIO];

	/*
	 * splhigh() must block "everything".
	 */
	pxa2x0_imask[IPL_HIGH] &= pxa2x0_imask[IPL_STATCLOCK];

	/*
	 * XXX We need serial drivers to run at the absolute highest priority
	 * in order to avoid overruns, so serial > high.
	 */
	pxa2x0_imask[IPL_SERIAL] &= pxa2x0_imask[IPL_HIGH];

	write_icu(SAIPIC_MR, pxa2x0_imask[current_spl_level]);

	restore_interrupts(psw);
}


void
pxa2x0_init_interrupt_masks(void)
{

	memset(pxa2x0_imask, 0, sizeof(pxa2x0_imask));

	/*
	 * IPL_NONE has soft interrupts enabled only, at least until
	 * hardware handlers are installed.
	 */
	pxa2x0_imask[IPL_NONE] =
	    SI_TO_IRQBIT(SI_SOFT) |
	    SI_TO_IRQBIT(SI_SOFTCLOCK) |
	    SI_TO_IRQBIT(SI_SOFTNET) |
	    SI_TO_IRQBIT(SI_SOFTSERIAL);

	/*
	 * Initialize the soft interrupt masks to block themselves.
	 */
	pxa2x0_imask[IPL_SOFT] = ~SI_TO_IRQBIT(SI_SOFT);
	pxa2x0_imask[IPL_SOFTCLOCK] = ~SI_TO_IRQBIT(SI_SOFTCLOCK);
	pxa2x0_imask[IPL_SOFTNET] = ~SI_TO_IRQBIT(SI_SOFTNET);
	pxa2x0_imask[IPL_SOFTSERIAL] = ~SI_TO_IRQBIT(SI_SOFTSERIAL);

	pxa2x0_imask[IPL_SOFT] &= pxa2x0_imask[IPL_NONE];

	/*
	 * splsoftclock() is the only interface that users of the
	 * generic software interrupt facility have to block their
	 * soft intrs, so splsoftclock() must also block IPL_SOFT.
	 */
	pxa2x0_imask[IPL_SOFTCLOCK] &= pxa2x0_imask[IPL_SOFT];

	/*
	 * splsoftnet() must also block splsoftclock(), since we don't
	 * want timer-driven network events to occur while we're
	 * processing incoming packets.
	 */
	pxa2x0_imask[IPL_SOFTNET] &= pxa2x0_imask[IPL_SOFTCLOCK];
}

void
pxa2x0_do_pending(void)
{
	static __cpu_simple_lock_t processing = __SIMPLELOCK_UNLOCKED;
	int oldirqstate, spl_save;

	if (__cpu_simple_lock_try(&processing) == 0)
		return;

	spl_save = current_spl_level;

	oldirqstate = disable_interrupts(I32_bit);

#if 1
#define	DO_SOFTINT(si,ipl)						\
	if ((softint_pending & intr_mask) & SI_TO_IRQBIT(si)) {		\
		softint_pending &= ~SI_TO_IRQBIT(si);			\
		if (current_spl_level < ipl)				\
			pxa2x0_setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
		pxa2x0_setipl(spl_save);					\
	}

	do {
		DO_SOFTINT(SI_SOFTSERIAL,IPL_SOFTSERIAL);
		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SI_SOFT, IPL_SOFT);
	} while( softint_pending & intr_mask );
#else
	while( (si = find_first_bit(softint_pending & intr_mask)) >= 0 ){
		softint_pending &= ~SI_TO_IRQBIT(si);
		if (current_spl_level < ipl)
			pxa2x0_setipl(ipl);
		restore_interrupts(oldirqstate);
		softintr_dispatch(si);
		oldirqstate = disable_interrupts(I32_bit);
		pxa2x0_setipl(spl_save);
	}
#endif

	__cpu_simple_unlock(&processing);

	restore_interrupts(oldirqstate);
}


#undef splx
void
splx(int ipl)
{

	pxa2x0_splx(ipl);
}

#undef _splraise
int
_splraise(int ipl)
{

	return pxa2x0_splraise(ipl);
}

#undef _spllower
int
_spllower(int ipl)
{

	return pxa2x0_spllower(ipl);
}

#undef _setsoftintr
void
_setsoftintr(int si)
{

	return pxa2x0_setsoftintr(si);
}

void *
pxa2x0_intr_establish(int irqno, int level,
    int (*func)(void *), void *arg, char *name)
{
	int psw;
	struct intrhandler *ih;

	if (irqno < PXA2X0_IRQ_MIN || irqno >= ICU_LEN)
		panic("intr_establish: bogus irq number %d", irqno);

	psw = disable_interrupts(I32_bit);

	ih = &handler[irqno];
	ih->arg = arg;
	ih->func = func;
	ih->name = name;
	ih->ih_irq = irqno;
	extirq_level[irqno] = level;

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, (void *)&ih->ih_irq,
		    &evcount_intr);

	pxa2x0_update_intr_masks(irqno, level);

	intr_mask = pxa2x0_imask[current_spl_level];
	
	restore_interrupts(psw);

	return (ih);
}
void
pxa2x0_intr_disestablish(void *cookie)
{
	struct intrhandler *lhandler = cookie;
	int irqno;
	int psw;
	struct intrhandler *ih;

	irqno = lhandler - handler;

	if (irqno < PXA2X0_IRQ_MIN || irqno >= ICU_LEN)
		panic("intr_disestablish: bogus irq number %d", irqno);

	psw = disable_interrupts(I32_bit);

	ih = &handler[irqno];
	if (ih->name != NULL)
		evcount_detach(&ih->ih_count);

	ih->arg = (void *) irqno;
	ih->func = pxa2x0_stray_interrupt;
	ih->name = "stray";
	extirq_level[irqno] = IPL_SERIAL;
	pxa2x0_update_intr_masks(irqno, IPL_SERIAL);

	intr_mask = pxa2x0_imask[current_spl_level];
	
	restore_interrupts(psw);
}

/*
 * Glue for drivers of sa11x0 compatible integrated logics.
 */
void *
sa11x0_intr_establish(sa11x0_chipset_tag_t ic, int irq, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, char *name)
{

	return pxa2x0_intr_establish(irq, level, ih_fun, ih_arg, name);
}
