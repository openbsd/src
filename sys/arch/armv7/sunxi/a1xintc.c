/*	$OpenBSD: a1xintc.c,v 1.4 2014/07/12 18:44:41 tedu Exp $	*/
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
#define DPRINTF(x)	do { if (intcdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (intcdebug>(n)) printf x; } while (0)
int	intcdebug = 10;
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

volatile int softint_pending;

struct intrq intc_handler[NIRQ];
u_int32_t intc_smask[NIPL];
u_int32_t intc_imask[NBANKS][NIPL];

bus_space_tag_t		intc_iot;
bus_space_handle_t	intc_ioh;
int			intc_nirq;

void	a1xintc_attach(struct device *, struct device *, void *);
int	intc_spllower(int);
int	intc_splraise(int);
void	intc_setipl(int);
void	intc_calc_masks(void);

struct cfattach	a1xintc_ca = {
	sizeof (struct device), NULL, a1xintc_attach
};

struct cfdriver a1xintc_cd = {
	NULL, "a1xintc", DV_DULL
};

int intc_attached = 0;

void
a1xintc_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	int i, j;

	intc_iot = aa->aa_iot;
	if (bus_space_map(intc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &intc_ioh))
		panic("a1xintc_attach: bus_space_map failed!");

	/* disable/mask/clear all interrupts */
	for (i = 0; i < NBANKS; i++) {
		bus_space_write_4(intc_iot, intc_ioh, INTC_ENABLE_REG(i), 0);
		bus_space_write_4(intc_iot, intc_ioh, INTC_MASK_REG(i), 0);
		bus_space_write_4(intc_iot, intc_ioh, INTC_IRQ_PENDING_REG(i),
		    0xffffffff);
		for (j = 0; j < NIPL; j++)
			intc_imask[i][j] = 0;
	}

	/* XXX */
	bus_space_write_4(intc_iot, intc_ioh, INTC_PROTECTION_REG, 1);
	bus_space_write_4(intc_iot, intc_ioh, INTC_NMI_CTRL_REG, 0);

	for (i = 0; i < NIRQ; i++)
		TAILQ_INIT(&intc_handler[i].iq_list);

	intc_calc_masks();

	arm_init_smask();
	intc_attached = 1;

	/* insert self as interrupt handler */
	arm_set_intr_handler(intc_splraise, intc_spllower, intc_splx,
	    intc_setipl,
	    intc_intr_establish, intc_intr_disestablish, intc_intr_string,
	    intc_irq_handler);
	intc_setipl(IPL_HIGH);  /* XXX ??? */
	enable_interrupts(I32_bit);
	printf("\n");
}

void
intc_calc_masks(void)
{
	struct cpu_info *ci = curcpu();
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
			    irq, max, min, IRQ2REG32(irq),
			    IRQ2BIT32(irq));
		}
#endif
		/* Enable interrupts at lower levels, clear -> enable */
		for (i = 0; i < min; i++)
			intc_imask[IRQ2REG32(irq)][i] &=
			    ~(1 << IRQ2BIT32(irq));
		for (; i < NIPL; i++)
			intc_imask[IRQ2REG32(irq)][i] |=
			    (1 << IRQ2BIT32(irq));
		/* XXX - set enable/disable, priority */ 
	}

	intc_setipl(ci->ci_cpl);
}

void
intc_splx(int new)
{
	struct cpu_info *ci = curcpu();
	intc_setipl(new);

	if (ci->ci_ipending & arm_smask[ci->ci_cpl])
		arm_do_pending_intr(ci->ci_cpl);
}

int
intc_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;
	intc_splx(new);
	return (old);
}

int
intc_splraise(int new)
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

	intc_setipl(new);
  
	return (old);
}

void
intc_setipl(int new)
{
	struct cpu_info *ci = curcpu();
	int i, psw;
#if 1
	/*
	 * XXX not needed, because all interrupts are disabled
	 * by default, so touching maskregs has no effect, i hope.
	 */
	if (intc_attached == 0) {
		ci->ci_cpl = new;
		return;
	}
#endif
	psw = disable_interrupts(I32_bit);
	ci->ci_cpl = new;
	for (i = 0; i < NBANKS; i++)
		bus_space_write_4(intc_iot, intc_ioh,
		    INTC_MASK_REG(i), intc_imask[i][new]);
	restore_interrupts(psw);
}

void
intc_irq_handler(void *frame)
{
	struct intrhand *ih;
	void *arg;
	uint32_t pr;
	int irq, prio, s;

	irq = bus_space_read_4(intc_iot, intc_ioh, INTC_VECTOR_REG) >> 2;
	if (irq == 0)
		return;
	if (irq == 1)
		sxipio_togglepin(SXIPIO_LED_BLUE);

	sxipio_setpin(SXIPIO_LED_GREEN);

	prio = intc_handler[irq].iq_irq;
	s = intc_splraise(prio);
	splassert(prio);

	pr = bus_space_read_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    pr & ~(1 << IRQ2BIT32(irq)));

	/* clear pending */
	pr = bus_space_read_4(intc_iot, intc_ioh,
	    INTC_IRQ_PENDING_REG(IRQ2REG32(irq)));
	bus_space_write_4(intc_iot, intc_ioh,
	    INTC_IRQ_PENDING_REG(IRQ2REG32(irq)),
	    pr | (1 << IRQ2BIT32(irq)));

	pr = bus_space_read_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    pr | (1 << IRQ2BIT32(irq)));

	TAILQ_FOREACH(ih, &intc_handler[irq].iq_list, ih_list) {
		if (ih->ih_arg != 0)
			arg = ih->ih_arg;
		else
			arg = frame;

		if (ih->ih_func(arg)) 
			ih->ih_count.ec_count++;
	}
	intc_splx(s);

	sxipio_clrpin(SXIPIO_LED_GREEN);
}

void *
intc_intr_establish(int irq, int lvl, int (*f)(void *), void *arg, char *name)
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

	TAILQ_INSERT_TAIL(&intc_handler[irq].iq_list, ih, ih_list);

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	er = bus_space_read_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    er | (1 << IRQ2BIT32(irq)));

	intc_calc_masks();
	
	restore_interrupts(psw);
	return (ih);
}

void
intc_intr_disestablish(void *cookie)
{
	struct intrhand *ih = cookie;
	int irq = ih->ih_irq;
	int psw;
	uint32_t er;

	psw = disable_interrupts(I32_bit);

	TAILQ_REMOVE(&intc_handler[irq].iq_list, ih, ih_list);

	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);

	free(ih, M_DEVBUF, 0);

	er = bus_space_read_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)));
	bus_space_write_4(intc_iot, intc_ioh,
	    INTC_ENABLE_REG(IRQ2REG32(irq)),
	    er & ~(1 << IRQ2BIT32(irq)));

	intc_calc_masks();

	restore_interrupts(psw);
}

const char *
intc_intr_string(void *cookie)
{
	return "asd?";
}
