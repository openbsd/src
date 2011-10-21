/* $OpenBSD: intc.c,v 1.7 2011/10/21 22:55:01 drahn Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <machine/bus.h>
#include <arch/beagle/beagle/ahb.h>

/* registers */
#define	INTC_REVISION		0x00	/* R */
#define	INTC_SYSCONFIG		0x10	/* RW */
#define		INTC_SYSCONFIG_AUTOIDLE		0x1
#define		INTC_SYSCONFIG_SOFTRESET	0x2
#define	INTC_SYSSTATUS		0x14	/* R */
#define		INTC_SYSSYSTATUS_RESETDONE	0x1
#define	INTC_SIR_IRQ		0x40	/* R */	
#define	INTC_SIR_FIQ		0x44	/* R */
#define	INTC_CONTROL		0x48	/* RW */
#define		INTC_CONTROL_NEWIRQ	0x1
#define		INTC_CONTROL_NEWFIQ	0x1
#define		INTC_CONTROL_GLOBALMASK	0x1
#define	INTC_PROTECTION		0x4c	/* RW */
#define		INTC_PROTECTION_PROT 1	/* only privileged mode */
#define	INTC_IDLE		0x50	/* RW */

#define INTC_IRQ_TO_REG(i)	(((i) >> 5) & 0x3)
#define INTC_IRQ_TO_REGi(i)	((i) & 0x1f)
#define	INTC_ITRn(i)		0x80+(0x20*i)+0x00	/* R */
#define	INTC_MIRn(i)		0x80+(0x20*i)+0x04	/* RW */
#define	INTC_CLEARn(i)		0x80+(0x20*i)+0x08	/* RW */
#define	INTC_SETn(i)		0x80+(0x20*i)+0x0c	/* RW */
#define	INTC_ISR_SETn(i)	0x80+(0x20*i)+0x10	/* RW */
#define	INTC_ISR_CLEARn(i)	0x80+(0x20*i)+0x14	/* RW */
#define INTC_PENDING_IRQn(i)	0x80+(0x20*i)+0x18	/* R */
#define INTC_PENDING_FIQn(i)	0x80+(0x20*i)+0x1c	/* R */

#define	INTC_ITR0		0x80	/* R */
#define	INTC_MIR0		0x84	/* RW */
#define	INTC_CLEAR0		0x88	/* RW */
#define	INTC_SET0		0x8c	/* RW */
#define	INTC_ISR_SET0		0x90	/* RW */
#define	INTC_ISR_CLEAR0		0x94	/* RW */
#define INTC_PENDING_IRQ0	0x98 	/* R */
#define INTC_PENDING_FIQ0	0x9c 	/* R */

#define	INTC_ITR1		0xa0	/* R */
#define	INTC_MIR1		0xa4	/* RW */
#define	INTC_CLEAR1		0xa8	/* RW */
#define	INTC_SET1		0xac	/* RW */
#define	INTC_ISR_SET1		0xb0	/* RW */
#define	INTC_ISR_CLEAR1		0xb4	/* RW */
#define INTC_PENDING_IRQ1	0xb8 	/* R */
#define INTC_PENDING_FIQ1	0xbc 	/* R */

#define	INTC_ITR2		0xc0	/* R */
#define	INTC_MIR2		0xc4	/* RW */
#define	INTC_CLEAR2		0xc8	/* RW */
#define	INTC_SET2		0xcc	/* RW */
#define	INTC_ISR_SET2		0xd0	/* RW */
#define	INTC_ISR_CLEAR2		0xd4	/* RW */
#define INTC_PENDING_IRQ2	0xd8 	/* R */
#define INTC_PENDING_FIQ2	0xdc 	/* R */

#define INTC_ILRn(i)		0x100+(4*i)
#define		INTC_ILR_IRQ	0x0		/* not of FIQ */
#define		INTC_ILR_FIQ	0x1
#define		INTC_ILR_PRIs(pri)	((pri) << 2)
#define		INTC_ILR_PRI(reg)	(((reg) >> 2) & 0x2f)
#define		INTC_MIN_PRI	63
#define		INTC_STD_PRI	32
#define		INTC_MAX_PRI	0

#define	INTC_SIZE		0x200

#define NIRQ	INTC_NUM_IRQ

struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	struct evcount	ih_count;
	char *ih_name;
};

struct intrq {
	TAILQ_HEAD(, intrhand) iq_list;	/* handler list */
	int iq_irq;			/* IRQ to mask while handling */
	int iq_levels;			/* IPL_*'s this IRQ has */
	int iq_ist;			/* share type */
};

volatile int current_ipl_level = IPL_HIGH;
volatile int softint_pending;

struct intrq intc_handler[NIRQ];
u_int32_t intc_smask[NIPL];
u_int32_t intc_imask[3][NIPL];

bus_space_tag_t		intc_iot;
bus_space_handle_t	intc_ioh;

int	intc_match(struct device *, void *, void *);
void	intc_attach(struct device *, struct device *, void *);
int	_spllower(int new);
int	_splraise(int new);
void	intc_setipl(int new);
void	intc_calc_mask(void);
void	intc_do_pending(void);

struct cfattach	intc_ca = {
	sizeof (struct device), intc_match, intc_attach
};

struct cfdriver intc_cd = {
	NULL, "intc", DV_DULL
};

int intc_attached = 0;

int
intc_match(struct device *parent, void *v, void *aux)
{
	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		break; /* continue trying */
	case BOARD_ID_OMAP4_PANDA:
		return 0; /* not ported yet ??? - different */
	default:
		return 0; /* unknown */
	}
	if (intc_attached != 0)
		return 0;

	/* XXX */
	return (1);
}

void
intc_attach(struct device *parent, struct device *self, void *args)
{
        struct ahb_attach_args *aa = args;
	int i;
	u_int32_t rev;

	intc_iot = aa->aa_iot;
	if (bus_space_map(intc_iot, aa->aa_addr, INTC_SIZE, 0, &intc_ioh))
		panic("intc_attach: bus_space_map failed!");

	rev = bus_space_read_4(intc_iot, intc_ioh, INTC_REVISION);

	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);

	/* software reset of the part? */
	/* set protection bit (kernel only)? */
#if 0
	bus_space_write_4(intc_iot, intc_ioh, INTC_PROTECTION,
	     INTC_PROTECTION_PROT);
#endif


	/* XXX - check power saving bit */


	/* mask all interrupts */
	bus_space_write_4(intc_iot, intc_ioh, INTC_MIR0, 0xffffffff);
	bus_space_write_4(intc_iot, intc_ioh, INTC_MIR1, 0xffffffff);
	bus_space_write_4(intc_iot, intc_ioh, INTC_MIR2, 0xffffffff);

	for (i = 0; i < NIRQ; i++) {
		bus_space_write_4(intc_iot, intc_ioh, INTC_ILRn(i),
		    INTC_ILR_PRIs(INTC_MIN_PRI)|INTC_ILR_IRQ);

		TAILQ_INIT(&intc_handler[i].iq_list);
	}
	intc_calc_mask();
	bus_space_write_4(intc_iot, intc_ioh, INTC_CONTROL,
	    INTC_CONTROL_NEWIRQ);

	intc_attached = 1;

	_splraise(IPL_HIGH);
	enable_interrupts(I32_bit);
}

void
intc_calc_mask(void)
{
	int irq;
	struct intrhand *ih;
	int i;

	for (irq = 0; irq < NIRQ; irq++) {
		int max = IPL_NONE;
		int min = IPL_HIGH;
		TAILQ_FOREACH(ih, &intc_handler[irq].iq_list, ih_list) {
			if (ih->ih_ipl > max)
				max = ih->ih_ipl;

			if (ih->ih_ipl < min)
				min = ih->ih_ipl;
		}

		intc_handler[irq].iq_irq = max;

		if (max == IPL_NONE)
			min = IPL_NONE;

#ifdef DEBUG_INTC
		if (min != IPL_NONE) {
			printf("irq %d to block at %d %d reg %d bit %d\n",
			    irq, max, min, INTC_IRQ_TO_REG(irq),
			    INTC_IRQ_TO_REGi(irq));
		}
#endif
		/* Enable interrupts at lower levels, clear -> enable */
		for (i = 0; i < min; i++)
			intc_imask[INTC_IRQ_TO_REG(irq)][i] &=
			    ~(1 << INTC_IRQ_TO_REGi(irq));
		for (; i <= IPL_HIGH; i++)
			intc_imask[INTC_IRQ_TO_REG(irq)][i] |=
			    1 << INTC_IRQ_TO_REGi(irq);
		/* XXX - set enable/disable, priority */ 
		bus_space_write_4(intc_iot, intc_ioh, INTC_ILRn(irq),
		    INTC_ILR_PRIs(NIPL-max)|INTC_ILR_IRQ);
	}
	for (i = IPL_NONE; i <= IPL_HIGH; i++) {
#if 0
		printf("calc_mask lvl %d val %x %x %x\n", i,
		    intc_imask[0][i], intc_imask[1][i], intc_imask[2][i] );
#endif
		intc_smask[i] = 0;
		if (i < IPL_SOFT)
			intc_smask[i] |= SI_TO_IRQBIT(SI_SOFT);
		if (i < IPL_SOFTCLOCK)
			intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTCLOCK);
		if (i < IPL_SOFTNET)
			intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTNET);
		if (i < IPL_SOFTTTY)
			intc_smask[i] |= SI_TO_IRQBIT(SI_SOFTTTY);
	}
	intc_setipl(current_ipl_level);
}

/*
 * XXX - is it possible to do the soft interrupts with actual interrupt
 * instead of emulating them?
 */
void
intc_do_pending(void)
{
	static int processing = 0;
	int oldirqstate, spl_save;

	oldirqstate = disable_interrupts(I32_bit);

	spl_save = current_ipl_level;

	if (processing == 1) {
		restore_interrupts(oldirqstate);
		return;
	}
//	printf("softint_pending %x\n", softint_pending);

#define DO_SOFTINT(si, ipl) \
	if ((softint_pending & intc_smask[current_ipl_level]) &	\
	    SI_TO_IRQBIT(si)) {						\
		softint_pending &= ~SI_TO_IRQBIT(si);			\
		if (current_ipl_level < ipl)				\
			intc_setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
		intc_setipl(spl_save);					\
	}

	do {
		DO_SOFTINT(SI_SOFTTTY, IPL_SOFTTTY);
		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SI_SOFT, IPL_SOFT);
	} while (softint_pending & intc_smask[current_ipl_level]);
		

#if 0
	printf("exit softint_pending %x pri %x mask %x\n", softint_pending,
	    current_ipl_level, intc_smask[current_ipl_level]);
#endif

	processing = 0;
	restore_interrupts(oldirqstate);
}

void
splx(int new)
{

	intc_setipl(new);

	if (softint_pending & intc_smask[current_ipl_level])
		intc_do_pending();
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

	intc_setipl(new);
  
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
	if (softint_pending & intc_smask[current_ipl_level])
		intc_do_pending();
}



void
intc_setipl(int new)
{
	int i;
	int psw;
	if (intc_attached == 0)
		return;

	psw = disable_interrupts(I32_bit);
	current_ipl_level = new;
	for (i = 0; i < 3; i++)
		bus_space_write_4(intc_iot, intc_ioh,
		    INTC_MIRn(i), intc_imask[i][new]);
	bus_space_write_4(intc_iot, intc_ioh, INTC_CONTROL,
	    INTC_CONTROL_NEWIRQ);
	restore_interrupts(psw);
}

void
intc_intr_bootstrap(vaddr_t addr)
{
	int i, j;
	extern struct bus_space armv7_bs_tag;
	intc_iot = &armv7_bs_tag;
	intc_ioh = addr;
	for (i = 0; i < 3; i++)
		for (j = 0; j < NIPL; j++)
			intc_imask[i][j] = 0xffffffff;
}

void
intc_irq_handler(void *frame)
{
	int irq, pri, s;
	struct intrhand *ih;
	void *arg;

	irq = bus_space_read_4(intc_iot, intc_ioh, INTC_SIR_IRQ);
#ifdef DEBUG_INTC
	printf("irq %d fired\n", irq);
#endif

	pri = intc_handler[irq].iq_irq;
	s = _splraise(pri);
	TAILQ_FOREACH(ih, &intc_handler[irq].iq_list, ih_list) {
		if (ih->ih_arg != 0)
			arg = ih->ih_arg;
		else
			arg = frame;

		if (ih->ih_func(arg)) 
			ih->ih_count.ec_count++;

	}
	bus_space_write_4(intc_iot, intc_ioh, INTC_CONTROL,
	    INTC_CONTROL_NEWIRQ);
	splx(s);	 /* XXX - handles pending */
}

void *
intc_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name)
{
	int psw;
	struct intrhand *ih;

	if (irqno < 0 || irqno >= NIRQ)
		panic("intc_intr_establish: bogus irqnumber %d: %s",
		     irqno, name);
	psw = disable_interrupts(I32_bit);

	/* no point in sleeping unless someone can free memory. */
	ih = (struct intrhand *)malloc (sizeof *ih, M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level;
	ih->ih_irq = irqno;
	ih->ih_name = name;

	TAILQ_INSERT_TAIL(&intc_handler[irqno].iq_list, ih, ih_list);

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_INTC
	printf("intc_intr_establish irq %d level %d [%s]\n", irqno, level,
	    name);
#endif
	intc_calc_mask();
	
	restore_interrupts(psw);
	return (ih);
}

void
intc_intr_disestablish(void *cookie)
{
	int psw;
	struct intrhand *ih = cookie;
	int irqno = ih->ih_irq;
	psw = disable_interrupts(I32_bit);
	TAILQ_REMOVE(&intc_handler[irqno].iq_list, ih, ih_list);
	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);
	free(ih, M_DEVBUF);
	restore_interrupts(psw);
}

#if 0
int intc_tst(void *a);

int
intc_tst(void *a)
{
	printf("inct_tst called\n");
	bus_space_write_4(intc_iot, intc_ioh, INTC_ISR_CLEAR0, 2);
	return 1;
}

void intc_test(void);
void intc_test(void)
{
	void * ih;
	printf("about to register handler\n");
	ih = intc_intr_establish(1, IPL_BIO, intc_tst, NULL, "intctst");

	printf("about to set bit\n");
	bus_space_write_4(intc_iot, intc_ioh, INTC_ISR_SET0, 2);

	printf("about to clear bit\n");
	bus_space_write_4(intc_iot, intc_ioh, INTC_ISR_CLEAR0, 2);

	printf("about to remove handler\n");
	intc_intr_disestablish(ih);

	printf("done\n");
}
#endif

#ifdef DIAGNOSTIC
void
intc_splassert_check(int wantipl, const char *func)
{
        int oldipl = current_ipl_level;

        if (oldipl < wantipl) {
                splassert_fail(wantipl, oldipl, func);
                /*
                 * If the splassert_ctl is set to not panic, raise the ipl
                 * in a feeble attempt to reduce damage.
                 */
                intc_setipl(wantipl);
        }
}
#endif

