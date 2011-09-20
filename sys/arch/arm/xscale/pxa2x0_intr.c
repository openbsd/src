/*	$OpenBSD: pxa2x0_intr.c,v 1.22 2011/09/20 22:02:13 miod Exp $ */
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/evcount.h>
#include <sys/queue.h>
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
#if 1
#define MULTIPLE_HANDLERS_ON_ONE_IRQ
#endif
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
struct intrhand {
	TAILQ_ENTRY(intrhand)	ih_list;		/* link on intrq list */
	int 			(*ih_func)(void *);	/* handler */
	void 			*ih_arg;		/* arg for handler */
	char 			*ih_name;
	struct evcount  	ih_count;
	int 			ih_irq;
	int 			ih_level;
};
#endif

static struct intrhandler{
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	TAILQ_HEAD(,intrhand) list;
#else
	pxa2x0_irq_handler_t func;
	char *name;
	void *arg;		/* NULL for stackframe */
	int ih_irq;
	struct evcount ih_count;
#endif
} handler[ICU_LEN];

__volatile int softint_pending;
__volatile int current_spl_level;
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
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
		TAILQ_INIT(&handler[i].list);
		extirq_level[i] = IPL_NONE;
#else
		handler[i].name = "stray";
		handler[i].func = pxa2x0_stray_interrupt;
		handler[i].arg = (void *)(u_int32_t) i;
		extirq_level[i] = IPL_HIGH;
#endif

	}

	pxa2x0_init_interrupt_masks();

	_splraise(IPL_HIGH);
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
 * PXA27x has MSL interface and SSP3 interrupts [0,1], USIM interface
 * and SSP2 interrupts [15,16]. PXA255 has bits [0..6,15] reserved and
 * bit [16] network SSP interrupt.  We don't need any of those, so we
 * map software interrupts to bits [0..1,15..16].  Sadly there are no
 * four contiguous bits safe enough to use on both processors.
 */
#define SI_TO_IRQBIT(si)  ((si) < 2 ? 1U<<(si) : 1U<<(15-2+(si)))

/*
 * Map a software interrupt queue to an interrupt priority level.
 */
static const int si_to_ipl[SI_NQUEUES] = {
	IPL_SOFT,		/* SI_SOFT */
	IPL_SOFTCLOCK,		/* SI_SOFTCLOCK */
	IPL_SOFTNET,		/* SI_SOFTNET */
	IPL_SOFTTTY,		/* SI_SOFTTTY */
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
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	struct intrhand *ih;
#endif

	saved_spl_level = current_spl_level;

	/* get pending IRQs */
	irqbits = read_icu(SAIPIC_IP);

	while ((irqno = find_first_bit(irqbits)) >= 0) {
		/* XXX: Should we handle IRQs in priority order? */

		/* raise spl to stop interrupts of lower priorities */
		if (saved_spl_level < extirq_level[irqno])
			pxa2x0_setipl(extirq_level[irqno]);

		/* Enable interrupt */
		enable_interrupts(I32_bit);

#ifndef MULTIPLE_HANDLERS_ON_ONE_IRQ
		(* handler[irqno].func)( 
			handler[irqno].arg == 0
			? frame : handler[irqno].arg );
		handler[irqno].ih_count.ec_count++;
#else
		TAILQ_FOREACH(ih, &handler[irqno].list, ih_list) {
			if ((ih->ih_func)( ih->ih_arg == 0
			    ? frame : ih->ih_arg))
				ih->ih_count.ec_count++;
		}
#endif
		
		/* Disable interrupt */
		disable_interrupts(I32_bit);

		irqbits &= ~(1<<irqno);
	}

	/* restore spl to that was when this interrupt happen */
	pxa2x0_setipl(saved_spl_level);
			
	if(softint_pending & pxa2x0_imask[current_spl_level])
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

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
void pxa2x0_update_intr_masks(void);

void
pxa2x0_update_intr_masks()
#else
void pxa2x0_update_intr_masks(int irqno, int level);

void
pxa2x0_update_intr_masks(int irqno, int irqlevel)
#endif
{
	int psw;

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	int irq;
#ifdef DEBUG
	int level;
#endif
	struct intrhand *ih;
	psw = disable_interrupts(I32_bit);

	/* First figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
		int i;
		int max = IPL_NONE;
		int min = IPL_HIGH;
		TAILQ_FOREACH(ih, &handler[irq].list, ih_list) {
			if (ih->ih_level > max)
				max = ih->ih_level;

			if (ih->ih_level < min)
				min = ih->ih_level;
		}

		extirq_level[irq] = max;

		if (min == IPL_HIGH)
			min = IPL_NONE;

		/* Enable interrupt at lower level */
		for(i = 0; i < min; ++i)
			pxa2x0_imask[i] |= (1 << irq);

		/* Disable interrupt at upper level */
		for( ; i < NIPL; ++i)
			pxa2x0_imask[i] &= ~(1 << irq);
	}

	/* fixup */
	pxa2x0_imask[IPL_NONE] |=
	    SI_TO_IRQBIT(SI_SOFT) |
	    SI_TO_IRQBIT(SI_SOFTCLOCK) |
	    SI_TO_IRQBIT(SI_SOFTNET) |
	    SI_TO_IRQBIT(SI_SOFTTTY);
	pxa2x0_imask[IPL_SOFT] |=
	    SI_TO_IRQBIT(SI_SOFTCLOCK) |
	    SI_TO_IRQBIT(SI_SOFTNET) |
	    SI_TO_IRQBIT(SI_SOFTTTY);
	pxa2x0_imask[IPL_SOFTCLOCK] |=
	    SI_TO_IRQBIT(SI_SOFTNET) |
	    SI_TO_IRQBIT(SI_SOFTTTY);
	pxa2x0_imask[IPL_SOFTNET] |=
	    SI_TO_IRQBIT(SI_SOFTTTY);
	pxa2x0_imask[IPL_SOFTTTY] |=
	    0;
#else
	int level; /* debug */
	int mask = 1U<<irqno;
	int i;
	psw = disable_interrupts(I32_bit);

	for(i = 0; i < irqlevel; ++i)
		pxa2x0_imask[i] |= mask; /* Enable interrupt at lower level */

	for( ; i < NIPL; ++i)
		pxa2x0_imask[i] &= ~mask; /* Disable interrupt at upper level */
#endif

	/*
	 * Enforce a hierarchy that gives "slow" device (or devices with
	 * limited input buffer space/"real-time" requirements) a better
	 * chance at not dropping data.
	 */
	pxa2x0_imask[IPL_BIO] &= pxa2x0_imask[IPL_SOFTNET];
	pxa2x0_imask[IPL_NET] &= pxa2x0_imask[IPL_BIO];
	pxa2x0_imask[IPL_SOFTTTY] &= pxa2x0_imask[IPL_NET];
	pxa2x0_imask[IPL_TTY] &= pxa2x0_imask[IPL_SOFTTTY];

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

#ifdef DEBUG
	for (level = IPL_NONE; level < NIPL; level++) {
		printf("imask %d, %x\n", level, pxa2x0_imask[level]);
	}
#endif

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	for (irq = 0; irq < ICU_LEN; irq++) {
		int max_irq = IPL_NONE;
		TAILQ_FOREACH(ih, &handler[irq].list, ih_list) {
			if (ih->ih_level > max_irq) 
				max_irq  = ih->ih_level;
		}
		extirq_level[irq] = max_irq;
	}
#endif

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
	    SI_TO_IRQBIT(SI_SOFTTTY);

	/*
	 * Initialize the soft interrupt masks to block themselves.
	 */
	pxa2x0_imask[IPL_SOFT] = ~SI_TO_IRQBIT(SI_SOFT);
	pxa2x0_imask[IPL_SOFTCLOCK] = ~SI_TO_IRQBIT(SI_SOFTCLOCK);
	pxa2x0_imask[IPL_SOFTNET] = ~SI_TO_IRQBIT(SI_SOFTNET);
	pxa2x0_imask[IPL_SOFTTTY] = ~SI_TO_IRQBIT(SI_SOFTTTY);

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
	if ((softint_pending & pxa2x0_imask[current_spl_level]) & 	\
	    SI_TO_IRQBIT(si)) {		\
		softint_pending &= ~SI_TO_IRQBIT(si);			\
		if (current_spl_level < ipl)				\
			pxa2x0_setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
		pxa2x0_setipl(spl_save);				\
	}

	do {
		DO_SOFTINT(SI_SOFTTTY,IPL_SOFTTTY);
		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SI_SOFT, IPL_SOFT);
	} while( softint_pending & pxa2x0_imask[current_spl_level] );
#else
	while( (si = find_first_bit(softint_pending & pxa2x0_imask[current_spl_level])) >= 0 ){
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
    int (*func)(void *), void *arg, const char *name)
{
	int psw;
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	struct intrhand *ih;
#else
	struct intrhandler *ih;
#endif

	if (irqno < PXA2X0_IRQ_MIN || irqno >= ICU_LEN)
		panic("intr_establish: bogus irq number %d", irqno);

	psw = disable_interrupts(I32_bit);

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	/* no point in sleeping unless someone can free memory. */
	ih = (struct intrhand *)malloc(sizeof *ih, M_DEVBUF, 
	    cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");
        ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_irq = irqno;

	TAILQ_INSERT_TAIL(&handler[irqno].list, ih, ih_list);
#else
	ih = &handler[irqno];
	ih->arg = arg;
	ih->func = func;
	ih->name = name;
	ih->ih_irq = irqno;
	extirq_level[irqno] = level;
#endif

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	pxa2x0_update_intr_masks();
#else
	pxa2x0_update_intr_masks(irqno, level);
#endif

	restore_interrupts(psw);

	return (ih);
}

void
pxa2x0_intr_disestablish(void *cookie)
{

#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	int psw;
	struct intrhand *ih = cookie;
	int irqno =  ih->ih_irq;

	psw = disable_interrupts(I32_bit);
	TAILQ_REMOVE(&handler[irqno].list, ih, ih_list);

	free(ih, M_DEVBUF);

	pxa2x0_update_intr_masks();

	restore_interrupts(psw);
#else
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
	extirq_level[irqno] = IPL_HIGH;
	pxa2x0_update_intr_masks(irqno, IPL_HIGH);

	restore_interrupts(psw);
#endif
}

/*
 * Glue for drivers of sa11x0 compatible integrated logic.
 */
void *
sa11x0_intr_establish(sa11x0_chipset_tag_t ic, int irq, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *name)
{

	return pxa2x0_intr_establish(irq, level, ih_fun, ih_arg, name);
}

void
pxa2x0_setipl(int new)
{
	u_int32_t intr_mask;

	intr_mask = pxa2x0_imask[new];
	current_spl_level = new;
	write_icu( SAIPIC_MR, intr_mask );
}


void
pxa2x0_splx(int new)
{
	int psw;

	psw = disable_interrupts(I32_bit);
	pxa2x0_setipl(new);
	restore_interrupts(psw);

	/* If there are pending software interrupts, process them. */
	if (softint_pending & pxa2x0_imask[current_spl_level])
		pxa2x0_do_pending();
}


int
pxa2x0_splraise(int ipl)
{
	int	old, psw;

	old = current_spl_level;
	if( ipl > current_spl_level ){
		psw = disable_interrupts(I32_bit);
		pxa2x0_setipl(ipl);
		restore_interrupts(psw);
	}

	return (old);
}

int
pxa2x0_spllower(int ipl)
{
	int old = current_spl_level;
	int psw = disable_interrupts(I32_bit);
	pxa2x0_splx(ipl);
	restore_interrupts(psw);
	return(old);
}

void
pxa2x0_setsoftintr(int si)
{
	softint_pending |=  SI_TO_IRQBIT(si);

	/* Process unmasked pending soft interrupts. */
	if ( softint_pending & pxa2x0_imask[current_spl_level] )
		pxa2x0_do_pending();
}

const char *
pxa2x0_intr_string(void *cookie)
{
#ifdef MULTIPLE_HANDLERS_ON_ONE_IRQ
	struct intrhand *ih = cookie;
#else
	struct intrhandler *lhandler = cookie;
#endif
	static char irqstr[32];

	if (ih == NULL)
		snprintf(irqstr, sizeof irqstr, "couldn't establish interrupt");
	else
		snprintf(irqstr, sizeof irqstr, "irq %ld", ih->ih_irq);

	return irqstr;
}

#ifdef DIAGNOSTIC
void
pxa2x0_splassert_check(int wantipl, const char *func)
{
	int oldipl = current_spl_level, psw;

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		psw = disable_interrupts(I32_bit);
		pxa2x0_setipl(wantipl);
		restore_interrupts(psw);
	}
}
#endif
