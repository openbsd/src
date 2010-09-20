/* $OpenBSD: i80321_intr.c,v 1.14 2010/09/20 06:33:47 matthew Exp $ */

/*
 * Copyright (c) 2006 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <uvm/uvm.h>	/* uvmexp */

#include <machine/intr.h>

#include <arm/cpufunc.h>
#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

/*
 * autoconf glue
 */
int		i80321intc_match(struct device *, void *, void *);
void		i80321intc_attach(struct device *, struct device *, void *);

/* internal functions */
static void	i80321intc_write_intctl(uint32_t mask);
void		i80321intc_write_steer(uint32_t mask);
uint32_t	i80321intc_read_intsrc(void);
void		i80321intc_calc_mask(void);
void		i80321intc_init(void);
void		i80321intc_intr_init(void);
static void	i80321intc_setipl(int new);
void		i80321intc_do_pending(void);

uint32_t	i80321intc_imask[NIPL];
uint32_t	i80321intc_smask[NIPL];

#define SI_TO_IRQBIT(x)	(1 << (x))

__volatile int current_ipl_level;
__volatile int softint_pending;

struct cfattach i80321intc_ca = {
	sizeof(struct device), i80321intc_match, i80321intc_attach
};
        
struct cfdriver i80321intc_cd = {
	NULL, "i80321intc", DV_DULL
};

int i80321intc_attached = 0;

int
i80321intc_match(struct device *parent, void *v, void *aux)
{
	if (i80321intc_attached == 0)
		return 1;

	i80321intc_attached = 1;
	return 0;
}

void
i80321intc_attach(struct device *parent, struct device *self, void *args)
{
	i80321intc_init();
}

static inline void
i80321intc_write_intctl(uint32_t mask)
{
	__asm__ volatile ("mcr p6, 0, %0, c0, c0, 0" : : "r" (mask));
}

void
i80321intc_write_steer(uint32_t mask)
{
	__asm__ volatile ("mcr p6, 0, %0, c4, c0, 0" : : "r" (mask));
}

uint32_t
i80321intc_read_intsrc(void)
{
	uint32_t mask;
	__asm__ volatile ("mrc p6, 0, %0, c8, c0, 0" : "=r" (mask));
	return mask;
}

static inline void
i80321intc_setipl(int new)
{
	int psw;

	psw = disable_interrupts(I32_bit);
	current_ipl_level = new;
	i80321intc_write_intctl(i80321intc_imask[new]);
	restore_interrupts(psw);
}


struct intrq i80321_handler[NIRQ];

/*
 * Recompute the irq mask bits.
 * Must be called with interrupts disabled.
 */
void
i80321intc_calc_mask(void)
{
	int irq;
	struct intrhand *ih;
	int i;

	for (irq = 0; irq < NIRQ; irq++) {
		int i;
		int max = IPL_NONE;
		int min = IPL_HIGH;
		TAILQ_FOREACH(ih, &i80321_handler[irq].iq_list, ih_list) {
			if (ih->ih_ipl > max)
				max = ih->ih_ipl;

			if (ih->ih_ipl < min)
				min = ih->ih_ipl;
		}

		i80321_handler[irq].iq_irq = max;

		if (max == IPL_NONE)
			min = IPL_NONE; /* interrupt not enabled */
#if 0
		printf("irq %d: min %x max %x\n", irq, min, max);
#endif

		/* Enable interrupts at lower levels */
		for (i = 0; i < min; i++)
			i80321intc_imask[i] |= (1 << irq);
		/* Disable interrupts at upper levels */
		for (;i <= IPL_HIGH; i++)
			i80321intc_imask[i] &= ~(1 << irq);
	}
	/* initialize soft interrupt mask */
	for (i = IPL_NONE; i <= IPL_HIGH; i++)  {
		i80321intc_smask[i] = 0;
		if (i < IPL_SOFT)
			i80321intc_smask[i] |= SI_TO_IRQBIT(SI_SOFT);
		if (i < IPL_SOFTCLOCK)
			i80321intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTCLOCK);
		if (i < IPL_SOFTNET)
			i80321intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTNET);
		if (i < IPL_SOFTTTY)
			i80321intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTTTY);
#if 0
		printf("mask[%d]: %x %x\n", i, i80321intc_smask[i],
		    i80321intc_imask[i]);
#endif
	}

	i80321intc_setipl(current_ipl_level);
}

void
i80321intc_do_pending(void)
{
	static int processing = 0;
	int oldirqstate, spl_save;

	oldirqstate = disable_interrupts(I32_bit);

	spl_save = current_ipl_level;

	if (processing == 1) {
		restore_interrupts(oldirqstate);
		return;
	}

#define DO_SOFTINT(si, ipl) \
	if ((softint_pending & i80321intc_smask[current_ipl_level]) &	\
	    SI_TO_IRQBIT(si)) {						\
		softint_pending &= ~SI_TO_IRQBIT(si);			\
		if (current_ipl_level < ipl)				\
			i80321intc_setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
		i80321intc_setipl(spl_save);				\
	}

	do {
		DO_SOFTINT(SI_SOFTTTY, IPL_SOFTTTY);
		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SI_SOFT, IPL_SOFT);
	} while (softint_pending & i80321intc_smask[current_ipl_level]);
		

	processing = 0;
	restore_interrupts(oldirqstate);
}

void
splx(int new)
{
	i80321intc_setipl(new);

	if (softint_pending & i80321intc_smask[current_ipl_level])
		i80321intc_do_pending();
}

int
_spllower(int new)
{
	int old = current_ipl_level;
	splx(new);
	return (old);
}

int
_splraise(int new)
{
	int old;
	old = current_ipl_level;

	/* 
	 * setipl must always be called because there is a race window
	 * where the variable is updated before the mask is set
	 * an interrupt occurs in that window without the mask always
	 * being set, the hardware might not get updated on the next
	 * splraise completely messing up spl protection.
	 */
	if (old > new)
		new = old;

	i80321intc_setipl(new);

	return (old);
}

void
_setsoftintr(int si)
{
	int oldirqstate;

        oldirqstate = disable_interrupts(I32_bit);
	softint_pending |= SI_TO_IRQBIT(si);
	restore_interrupts(oldirqstate);

	/* Process unmasked pending soft interrupts. */
	if (softint_pending & i80321intc_smask[current_ipl_level])
		i80321intc_do_pending();
}

/*
 * i80321_icu_init:
 *
 *	Initialize the i80321 ICU.  Called early in bootstrap
 *	to make sure the ICU is in a pristine state.
 */
void
i80321intc_intr_init(void)
{
	i80321intc_write_intctl(0);

	i80321intc_write_steer(0);
}
	   
/*
 * i80321_intr_init:
 *
 *      Initialize the rest of the interrupt subsystem, making it
 *      ready to handle interrupts from devices.  
 */     
void
i80321intc_init(void)
{
	struct intrq *iq;
	int i;
			     
	for (i = 0; i < NIRQ; i++) {
		iq = &i80321_handler[i];
		TAILQ_INIT(&iq->iq_list);
	}       
 
	i80321intc_calc_mask();
      
	/* Enable IRQs (don't yet use FIQs). */
	enable_interrupts(I32_bit);
}

void *
i80321_intr_establish(int irq, int ipl, int (*func)(void *), void *arg,
    const char *name)
{
	struct intrq *iq;
	struct intrhand *ih;
	uint32_t oldirqstate;

	if (irq < 0 || irq > NIRQ)
		panic("i80321_intr_establish: IRQ %d out of range", irq);

	ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);
		  
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	ih->ih_name = name;
	ih->ih_irq = irq;
	   
	iq = &i80321_handler[irq];

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	/* All IOP321 interrupts are level-triggered. */
	iq->iq_ist = IST_LEVEL;

	oldirqstate = disable_interrupts(I32_bit);
	
	TAILQ_INSERT_TAIL(&iq->iq_list, ih, ih_list);
 
	i80321intc_calc_mask();

	restore_interrupts(oldirqstate);
			     
	return (ih);
}


void
i80321_intr_disestablish(void *cookie)   
{
	struct intrhand *ih = cookie;
	struct intrq *iq = &i80321_handler[ih->ih_irq];
	int oldirqstate;
		
	oldirqstate = disable_interrupts(I32_bit);

	TAILQ_REMOVE(&iq->iq_list, ih, ih_list);
	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);

	i80321intc_calc_mask();

	restore_interrupts(oldirqstate);
}

void  
i80321_irq_handler(void *arg)
{
	struct clockframe *frame = arg;
	uint32_t hwpend;
	int irq;
	int saved_spl_level;
	struct intrhand *ih;
	
	saved_spl_level = current_ipl_level;

	/* get pending IRQs */
	hwpend = i80321intc_read_intsrc();

	while ((irq = find_first_bit(hwpend)) >= 0) {
		/* XXX: Should we handle IRQs in priority order? */
 
		/* raise spl to stop interrupts of lower priorities */
		if (saved_spl_level < i80321_handler[irq].iq_irq)
			i80321intc_setipl(i80321_handler[irq].iq_irq);

		/* Enable interrupt */
		enable_interrupts(I32_bit);
		TAILQ_FOREACH(ih, &i80321_handler[irq].iq_list, ih_list) {
			if ((ih->ih_func)( ih->ih_arg == 0
			    ? frame : ih->ih_arg))
				ih->ih_count.ec_count++;
		}
		/* Disable interrupt */
		disable_interrupts(I32_bit);
		hwpend &= ~(1<<irq);
	}
	uvmexp.intrs++;

	/* restore spl to that was when this interrupt happen */
	i80321intc_setipl(saved_spl_level);

	if(softint_pending & i80321intc_smask[current_ipl_level])
		i80321intc_do_pending();
}

#ifdef DIAGNOSTIC
void
i80321_splassert_check(int wantipl, const char *func)
{
	int oldipl = current_ipl_level;

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		i80321intc_setipl(wantipl);
	}
}
#endif
