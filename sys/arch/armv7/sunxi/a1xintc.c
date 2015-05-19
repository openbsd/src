/*	$OpenBSD: a1xintc.c,v 1.5 2015/05/19 06:09:35 jsg Exp $	*/
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2013 Artturi Alm
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

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sunxireg.h>
#include <armv7/sunxi/sxipiovar.h>
#include <armv7/sunxi/a1xintc.h>

#ifdef DEBUG_INTC
#define DPRINTF(x)	do { if (a1xintcdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (a1xintcdebug>(n)) printf x; } while (0)
int	a1xintcdebug = 10;
char *ipl_strtbl[NIPL] = {
	"IPL_NONE",
	"IPL_SOFT",
	"IPL_SOFTCLOCK",
	"IPL_SOFTNET",
	"IPL_SOFTTTY",
	"IPL_BIO|IPL_USB",
	"IPL_NET",
	"IPL_TTY",
	"IPL_VM",
	"IPL_AUDIO",
	"IPL_CLOCK",
	"IPL_STATCLOCK",
	"IPL_SCHED|IPL_HIGH"
};
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define NIRQ			96
#define NBANKS			3
#define NIRQPRIOREGS		5

/* registers */
#define INTC_VECTOR_REG		0x00
#define INTC_BASE_ADR_REG	0x04
#define INTC_PROTECTION_REG	0x08
#define INTC_NMI_CTRL_REG	0x0c

#define INTC_IRQ_PENDING_REG0	0x10
#define INTC_IRQ_PENDING_REG1	0x14
#define INTC_IRQ_PENDING_REG2	0x18

#define INTC_SELECT_REG0	0x30
#define INTC_SELECT_REG1	0x34
#define INTC_SELECT_REG2	0x38

#define INTC_ENABLE_REG0	0x40
#define INTC_ENABLE_REG1	0x44
#define INTC_ENABLE_REG2	0x48

#define INTC_MASK_REG0		0x50
#define INTC_MASK_REG1		0x54
#define INTC_MASK_REG2		0x58

#define INTC_RESP_REG0		0x60
#define INTC_RESP_REG1		0x64
#define INTC_RESP_REG2		0x68

#define INTC_PRIO_REG0		0x80
#define INTC_PRIO_REG1		0x84
#define INTC_PRIO_REG2		0x88
#define INTC_PRIO_REG3		0x8c
#define INTC_PRIO_REG4		0x90

#define INTC_IRQ_PENDING_REG(_b)	(0x10 + ((_b) * 4))
#define INTC_FIQ_PENDING_REG(_b)	(0x20 + ((_b) * 4))
#define INTC_SELECT_REG(_b)		(0x30 + ((_b) * 4))
#define INTC_ENABLE_REG(_b)		(0x40 + ((_b) * 4))
#define INTC_MASK_REG(_b)		(0x50 + ((_b) * 4))
#define INTC_RESP_REG(_b)		(0x60 + ((_b) * 4))
#define INTC_PRIO_REG(_b)		(0x80 + ((_b) * 4))

#define IRQ2REG32(i)		(((i) >> 5) & 0x3)
#define IRQ2BIT32(i)		((i) & 0x1f)

#define IRQ2REG16(i)		(((i) >> 4) & 0x5)
#define IRQ2BIT16(i)		(((i) & 0x0f) * 2)

#define INTC_IRQ_HIPRIO		0x3
#define INTC_IRQ_ENABLED	0x2
#define INTC_IRQ_DISABLED	0x1
#define INTC_IRQ_LOWPRIO	0x0
#define INTC_PRIOCLEAR(i)	(~(INTC_IRQ_HIPRIO << IRQ2BIT16((i))))
#define INTC_PRIOENABLE(i)	(INTC_IRQ_ENABLED << IRQ2BIT16((i)))
#define INTC_PRIOHI(i)		(INTC_IRQ_HIPRIO << IRQ2BIT16((i)))


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

volatile int a1xsoftint_pending;

struct intrq a1xintc_handler[NIRQ];
u_int32_t a1xintc_smask[NIPL];
u_int32_t a1xintc_imask[NBANKS][NIPL];

bus_space_tag_t		a1xintc_iot;
bus_space_handle_t	a1xintc_ioh;
int			a1xintc_nirq;

void	a1xintc_attach(struct device *, struct device *, void *);
int	a1xintc_spllower(int);
int	a1xintc_splraise(int);
void	a1xintc_setipl(int);
void	a1xintc_calc_masks(void);

struct cfattach	a1xintc_ca = {
	sizeof (struct device), NULL, a1xintc_attach
};

struct cfdriver a1xintc_cd = {
	NULL, "a1xintc", DV_DULL
};

int a1xintc_attached = 0;

void
a1xintc_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	int i, j;

	a1xintc_iot = aa->aa_iot;
	if (bus_space_map(a1xintc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &a1xintc_ioh))
		panic("a1xintc_attach: bus_space_map failed!");

	/* disable/mask/clear all interrupts */
	for (i = 0; i < NBANKS; i++) {
		bus_space_write_4(a1xintc_iot, a1xintc_ioh, INTC_ENABLE_REG(i), 0);
		bus_space_write_4(a1xintc_iot, a1xintc_ioh, INTC_MASK_REG(i), 0);
		bus_space_write_4(a1xintc_iot, a1xintc_ioh, INTC_IRQ_PENDING_REG(i),
		    0xffffffff);
		for (j = 0; j < NIPL; j++)
			a1xintc_imask[i][j] = 0;
	}

	/* XXX */
	bus_space_write_4(a1xintc_iot, a1xintc_ioh, INTC_PROTECTION_REG, 1);
	bus_space_write_4(a1xintc_iot, a1xintc_ioh, INTC_NMI_CTRL_REG, 0);

	for (i = 0; i < NIRQ; i++)
		TAILQ_INIT(&a1xintc_handler[i].iq_list);

	a1xintc_calc_masks();

	arm_init_smask();
	a1xintc_attached = 1;

	/* insert self as interrupt handler */
	arm_set_intr_handler(a1xintc_splraise, a1xintc_spllower, a1xintc_splx,
	    a1xintc_setipl,
	    a1xintc_intr_establish, a1xintc_intr_disestablish, a1xintc_intr_string,
	    a1xintc_irq_handler);
	a1xintc_setipl(IPL_HIGH);  /* XXX ??? */
	enable_interrupts(I32_bit);
	printf("\n");
}

void
a1xintc_calc_masks(void)
{
	struct cpu_info *ci = curcpu();
	int irq;
	struct intrhand *ih;
	int i;

	for (irq = 0; irq < NIRQ; irq++) {
		int max = IPL_NONE;
		int min = IPL_HIGH;
		TAILQ_FOREACH(ih, &a1xintc_handler[irq].iq_list, ih_list) {
			if (ih->ih_ipl > max)
				max = ih->ih_ipl;
			if (ih->ih_ipl < min)
				min = ih->ih_ipl;
		}

		a1xintc_handler[irq].iq_irq = max;

		if (max == IPL_NONE)
			min = IPL_NONE;

#ifdef DEBUG_INTC
		if (min != IPL_NONE) {
			printf("irq %d to block at %d %d reg %d bit %d\n",
			    irq, max, min, IRQ2REG32(irq),
			    IRQ2BIT32(irq));
		}
#endif
		/* Enable interrupts at lower levels, clear -> enable */
		for (i = 0; i < min; i++)
			a1xintc_imask[IRQ2REG32(irq)][i] &=
			    ~(1 << IRQ2BIT32(irq));
		for (; i < NIPL; i++)
			a1xintc_imask[IRQ2REG32(irq)][i] |=
			    (1 << IRQ2BIT32(irq));
		/* XXX - set enable/disable, priority */ 
	}

	a1xintc_setipl(ci->ci_cpl);
}

void
a1xintc_splx(int new)
{
	struct cpu_info *ci = curcpu();
	a1xintc_setipl(new);

	if (ci->ci_ipending & arm_smask[ci->ci_cpl])
		arm_do_pending_intr(ci->ci_cpl);
}

int
a1xintc_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;
	a1xintc_splx(new);
	return (old);
}

int
a1xintc_splraise(int new)
{
	struct cpu_info *ci = curcpu();
	int old;
	old = ci->ci_cpl;

	/*
	 * setipl must always be called because there is a race window
	 * where the variable is updated before the mask is set
	 * an interrupt occurs in that window without the mask always
	 * being set, the hardware might not get updated on the next
	 * splraise completely messing up spl protection.
	 */
	if (old > new)
		new = old;

	a1xintc_setipl(new);
  
	return (old);
}

void
a1xintc_setipl(int new)
{
	struct cpu_info *ci = curcpu();
	int i, psw;
#if 1
	/*
	 * XXX not needed, because all interrupts are disabled
	 * by default, so touching maskregs has no effect, i hope.
	 */
	if (a1xintc_attached == 0) {
		ci->ci_cpl = new;
		return;
	}
#endif
	psw = disable_interrupts(I32_bit);
	ci->ci_cpl = new;
	for (i = 0; i < NBANKS; i++)
		bus_space_write_4(a1xintc_iot, a1xintc_ioh,
		    INTC_MASK_REG(i), a1xintc_imask[i][new]);
	restore_interrupts(psw);
}

void
a1xintc_irq_handler(void *frame)
{
	struct intrhand *ih;
	void *arg;
	uint32_t pr;
	int irq, prio, s;

	irq = bus_space_read_4(a1xintc_iot, a1xintc_ioh, INTC_VECTOR_REG) >> 2;
	if (irq == 0)
		return;
	if (irq == 1)
		sxipio_togglepin(SXIPIO_LED_BLUE);

	sxipio_setpin(SXIPIO_LED_GREEN);

	prio = a1xintc_handler[irq].iq_irq;
	s = a1xintc_splraise(prio);
	splassert(prio);

	pr = bus_space_read_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    pr & ~(1 << IRQ2BIT32(irq)));

	/* clear pending */
	pr = bus_space_read_4(a1xintc_iot, a1xintc_ioh,
	    INTC_IRQ_PENDING_REG(IRQ2REG32(irq)));
	bus_space_write_4(a1xintc_iot, a1xintc_ioh,
	    INTC_IRQ_PENDING_REG(IRQ2REG32(irq)),
	    pr | (1 << IRQ2BIT32(irq)));

	pr = bus_space_read_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    pr | (1 << IRQ2BIT32(irq)));

	TAILQ_FOREACH(ih, &a1xintc_handler[irq].iq_list, ih_list) {
		if (ih->ih_arg != 0)
			arg = ih->ih_arg;
		else
			arg = frame;

		if (ih->ih_func(arg)) 
			ih->ih_count.ec_count++;
	}
	a1xintc_splx(s);

	sxipio_clrpin(SXIPIO_LED_GREEN);
}

void *
a1xintc_intr_establish(int irq, int lvl, int (*f)(void *), void *arg, char *name)
{
	int psw;
	struct intrhand *ih;
	uint32_t er;

	if (irq <= 0 || irq >= NIRQ)
		panic("intr_establish: bogus irq %d %s\n", irq, name);

	DPRINTF(("intr_establish: irq %d level %d [%s]\n", irq, lvl,
	    name != NULL ? name : "NULL"));

	psw = disable_interrupts(I32_bit);

	/* no point in sleeping unless someone can free memory. */
	ih = (struct intrhand *)malloc (sizeof *ih, M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info\n");
	ih->ih_func = f;
	ih->ih_arg = arg;
	ih->ih_ipl = lvl;
	ih->ih_irq = irq;
	ih->ih_name = name;

	TAILQ_INSERT_TAIL(&a1xintc_handler[irq].iq_list, ih, ih_list);

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	er = bus_space_read_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    er | (1 << IRQ2BIT32(irq)));

	a1xintc_calc_masks();
	
	restore_interrupts(psw);
	return (ih);
}

void
a1xintc_intr_disestablish(void *cookie)
{
	struct intrhand *ih = cookie;
	int irq = ih->ih_irq;
	int psw;
	uint32_t er;

	psw = disable_interrupts(I32_bit);

	TAILQ_REMOVE(&a1xintc_handler[irq].iq_list, ih, ih_list);

	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);

	free(ih, M_DEVBUF, 0);

	er = bus_space_read_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(a1xintc_iot, a1xintc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    er & ~(1 << IRQ2BIT32(irq)));

	a1xintc_calc_masks();

	restore_interrupts(psw);
}

const char *
a1xintc_intr_string(void *cookie)
{
	return "asd?";
}
