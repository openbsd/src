/*	$OpenBSD: xheart.c,v 1.7 2009/05/08 18:42:07 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

/*
 * IP30 Heart Widget
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>
#include <sgi/xbow/xheartreg.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

struct xheart_softc {
	struct device		sc_dev;
	struct onewire_bus	sc_bus;

	uint64_t		sc_intrmask;
};

int	xheart_match(struct device *, void *, void *);
void	xheart_attach(struct device *, struct device *, void *);

const struct cfattach xheart_ca = {
	sizeof(struct xheart_softc), xheart_match, xheart_attach,
};

struct cfdriver xheart_cd = {
	NULL, "xheart", DV_DULL,
};

int	xheart_ow_reset(void *);
int	xheart_ow_read_bit(struct xheart_softc *);
int	xheart_ow_send_bit(void *, int);
int	xheart_ow_read_byte(void *);
int	xheart_ow_triplet(void *, int);
int	xheart_ow_pulse(struct xheart_softc *, int, int);

int	xheart_intr_register(int, int, int *);
int	xheart_intr_establish(int (*)(void *), void *, int, int, const char *);
void	xheart_intr_disestablish(int);
intrmask_t xheart_intr_handler(intrmask_t, struct trap_frame *);
void	xheart_intr_makemasks(struct xheart_softc *);
void	xheart_do_pending_int(int);

int
xheart_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI4 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI4_HEART) {
		/*
		 * Only match if no interrupt widget has registered yet.
		 * There should be only one Heart in a system anyway.
		 */
		return xbow_intr_widget == 0 ? 20 : 0;
	}

	return 0;
}

void
xheart_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	struct xheart_softc *sc = (void *)self;
	struct onewirebus_attach_args oba;
	paddr_t heart;

	printf(" revision %d\n", xaa->xaa_revision);

	sc->sc_bus.bus_cookie = sc;
	sc->sc_bus.bus_reset = xheart_ow_reset;
	sc->sc_bus.bus_bit = xheart_ow_send_bit;
	sc->sc_bus.bus_read_byte = xheart_ow_read_byte;
	sc->sc_bus.bus_write_byte = NULL;	/* use default routine */
	sc->sc_bus.bus_read_block = NULL;	/* use default routine */
	sc->sc_bus.bus_write_block = NULL;	/* use default routine */
	sc->sc_bus.bus_triplet = xheart_ow_triplet;
	sc->sc_bus.bus_matchrom = NULL;		/* use default routine */
	sc->sc_bus.bus_search = NULL;		/* use default routine */

	oba.oba_bus = &sc->sc_bus;
	oba.oba_flags = ONEWIRE_SCAN_NOW | ONEWIRE_NO_PERIODIC_SCAN;
	config_found(self, &oba, onewirebus_print);

	/*
	 * If no other widget has claimed interrupts routing, do it now.
	 */
	if (xbow_intr_widget == 0) {
		xbow_intr_widget = xaa->xaa_widget;
		xbow_intr_widget_register = 0x80;
		xbow_intr_widget_intr_register = xheart_intr_register;
		xbow_intr_widget_intr_establish = xheart_intr_establish;
		xbow_intr_widget_intr_disestablish = xheart_intr_disestablish;
		sc->sc_intrmask = 0;

		/*
		 * Acknowledge and disable all interrupts.
		 */
		heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
		*(volatile uint64_t*)(heart + HEART_ISR_CLR) =
		    0xffffffffffffffff;
		*(volatile uint64_t*)(heart + HEART_IMR(0)) = 0UL;
		*(volatile uint64_t*)(heart + HEART_IMR(1)) = 0UL;
		*(volatile uint64_t*)(heart + HEART_IMR(2)) = 0UL;
		*(volatile uint64_t*)(heart + HEART_IMR(3)) = 0UL;

		set_intr(INTPRI_XBOWMUX, CR_INT_0, xheart_intr_handler);
		register_pending_int_handler(xheart_do_pending_int);
	}
}

/*
 * Number-In-a-Can (1-Wire) interface
 */

int
xheart_ow_reset(void *v)
{
	struct xheart_softc *sc = v;
	return xheart_ow_pulse(sc, 500, 65);
}

int
xheart_ow_read_bit(struct xheart_softc *sc)
{
	return xheart_ow_pulse(sc, 6, 13);
}

int
xheart_ow_send_bit(void *v, int bit)
{
	struct xheart_softc *sc = v;
	int rc;
	
	if (bit != 0)
		rc = xheart_ow_pulse(sc, 6, 110);
	else
		rc = xheart_ow_pulse(sc, 80, 30);
	return rc;
}

int
xheart_ow_read_byte(void *v)
{
	struct xheart_softc *sc = v;
	unsigned int byte = 0;
	int i;

	for (i = 0; i < 8; i++)
		byte |= xheart_ow_read_bit(sc) << i;

	return byte;
}

int
xheart_ow_triplet(void *v, int dir)
{
	struct xheart_softc *sc = v;
	int rc;

	rc = xheart_ow_read_bit(sc);
	rc <<= 1;
	rc |= xheart_ow_read_bit(sc);

	switch (rc) {
	case 0x0:
		xheart_ow_send_bit(v, dir);
		break;
	case 0x1:
		xheart_ow_send_bit(v, 0);
		break;
	default:
		xheart_ow_send_bit(v, 1);
		break;
	}

	return (rc);
}

int
xheart_ow_pulse(struct xheart_softc *sc, int pulse, int data)
{
	uint64_t mcr_value;
	paddr_t heart;

	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE + HEART_MICROLAN, CCA_NC);
	mcr_value = (pulse << 10) | (data << 2);
	*(volatile uint64_t *)heart = mcr_value;
	do {
		mcr_value = *(volatile uint64_t *)heart;
	} while ((mcr_value & 0x00000002) == 0);

	delay(500);

	return (mcr_value & 1);
}

/*
 * HEART interrupt handling routines
 */

/*
 * Find a suitable interrupt bit for the given interrupt.
 */
int
xheart_intr_register(int widget, int level, int *intrbit)
{
	struct xheart_softc *sc = (void *)xheart_cd.cd_devs[0];
	int bit;

	/*
	 * All interrupts will be serviced at hardware level 0,
	 * so the `level' argument can be ignored.
	 */
	for (bit = HEART_INTR_WIDGET_MAX; bit >= HEART_INTR_WIDGET_MIN; bit--)
		if ((sc->sc_intrmask & (1 << bit)) == 0)
			break;

	if (bit < HEART_INTR_WIDGET_MIN)
		return EINVAL;

	*intrbit = bit;
	return 0;
}

/*
 * Register an interrupt handler for a given source, and enable it.
 */
int
xheart_intr_establish(int (*func)(void *), void *arg, int intrbit,
    int level, const char *name)
{
	struct xheart_softc *sc = (void *)xheart_cd.cd_devs[0];
	struct intrhand *ih;
	paddr_t heart;

#ifdef DIAGNOSTIC
	if (intrbit < HEART_INTR_MIN || intrbit > HEART_INTR_MAX)
		return EINVAL;
#endif

	/*
	 * HEART interrupts are not supposed to be shared - the interrupt
	 * mask is large enough for all widgets.
	 */
	if (intrhand[intrbit] != NULL)
		return EEXIST;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return ENOMEM;

	ih->ih_next = NULL;
	ih->ih_fun = func;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_irq = intrbit;
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_level,
		    &evcount_intr);
	intrhand[intrbit] = ih;

	sc->sc_intrmask |= 1UL << intrbit;
	xheart_intr_makemasks(sc);

	/* XXX this assumes we run on cpu0 */
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	*(volatile uint64_t *)(heart + HEART_IMR(0)) |= 1UL << intrbit;

	return 0;
}

void
xheart_intr_disestablish(int intrbit)
{
	struct xheart_softc *sc = (void *)xheart_cd.cd_devs[0];
	struct intrhand *ih;
	paddr_t heart;
	int s;

#ifdef DIAGNOSTIC
	if (intrbit < HEART_INTR_MIN || intrbit > HEART_INTR_MAX)
		return;
#endif

	s = splhigh();

	if ((ih = intrhand[intrbit]) == NULL) {
		splx(s);
		return;
	}

	/* XXX this assumes we run on cpu0 */
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	*(volatile uint64_t *)(heart + HEART_IMR(0)) &= ~(1UL << intrbit);

	intrhand[intrbit] = NULL;

	sc->sc_intrmask &= ~(1UL << intrbit);
	xheart_intr_makemasks(sc);

	free(ih, M_DEVBUF);

	splx(s);
}

/*
 * Xheart interrupt handler driver.
 */

uint64_t heart_intem = 0;

/*
 * Recompute interrupt masks.
 */
void
xheart_intr_makemasks(struct xheart_softc *sc)
{
	int irq, level;
	struct intrhand *q;
	intrmask_t intrlevel[INTMASKSIZE];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < INTMASKSIZE; irq++) {
		int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = IPL_NONE; level < NIPLS; level++) {
		int irqs = 0;
		for (irq = 0; irq < INTMASKSIZE; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		if (level != IPL_NONE)
			irqs |= SINT_ALLMASK;
		imask[level] = irqs;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_NET] |= imask[IPL_BIO];
	imask[IPL_TTY] |= imask[IPL_NET];
	imask[IPL_VM] |= imask[IPL_TTY];
	imask[IPL_CLOCK] |= imask[IPL_VM] | SPL_CLOCKMASK;

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0;
	imask[IPL_HIGH] = -1;

	heart_intem = sc->sc_intrmask;
	hw_setintrmask(0);
}

void
xheart_do_pending_int(int newcpl)
{
	/* Update masks to new cpl. Order highly important! */
	__asm__ (" .set noreorder\n");
	cpl = newcpl;
	__asm__ (" sync\n .set reorder\n");
	hw_setintrmask(newcpl);
	/* If we still have softints pending trigger processing. */
	if (ipending & SINT_ALLMASK & ~newcpl)
		setsoftintr0();
}

intrmask_t
xheart_intr_handler(intrmask_t hwpend, struct trap_frame *frame)
{
	paddr_t heart;
	uint64_t imr, isr;
	int icpl;
	int bit;
	intrmask_t mask;
	struct intrhand *ih;
	int rc;

	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	isr = *(volatile uint64_t *)(heart + HEART_ISR);
	imr = *(volatile uint64_t *)(heart + HEART_IMR(0));

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	*(volatile uint64_t *)(heart + HEART_IMR(0)) &= ~isr;

	/*
	 * If interrupts are spl-masked, mark them as pending only.
	 */
	if ((mask = isr & frame->cpl) != 0) {
		atomic_setbits_int(&ipending, mask);
		isr &= ~mask;
	}

	/*
	 * Now process unmasked interrupts.
	 */
	if (isr != 0) {
		atomic_clearbits_int(&ipending, isr);

		__asm__ (" .set noreorder\n");
		icpl = cpl;
		__asm__ (" sync\n .set reorder\n");

		/* XXX Rework this to dispatch in decreasing levels */
		for (bit = HEART_INTR_MAX, mask = 1 << bit;
		    bit >= HEART_INTR_MIN; bit--, mask >>= 1) {
			if ((isr & mask) == 0)
				continue;

			rc = 0;
			for (ih = intrhand[bit]; ih != NULL; ih = ih->ih_next) {
				splraise(imask[ih->ih_level]);
				ih->frame = frame;
				if ((*ih->ih_fun)(ih->ih_arg) != 0) {
					rc = 1;
					ih->ih_count.ec_count++;
				}
			}
			if (rc == 0)
				printf("spurious interrupt, source %d\n", bit);
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		*(volatile uint64_t *)(heart + HEART_IMR(0)) |= isr;

		__asm__ (" .set noreorder\n");
		cpl = icpl;
		__asm__ (" sync\n .set reorder\n");
	}

	return CR_INT_0;
}
