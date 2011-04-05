/*	$OpenBSD: xheart.c,v 1.21 2011/04/05 14:43:11 miod Exp $	*/

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
int	xheart_intr_establish(int (*)(void *), void *, int, int, const char *,
	    struct intrhand *);
void	xheart_intr_disestablish(int);
void	xheart_intr_clear(int);
void	xheart_intr_set(int);
uint32_t xheart_intr_handler(uint32_t, struct trap_frame *);
void	xheart_intr_makemasks(void);
void	xheart_setintrmask(int);
void	xheart_splx(int);

extern uint32_t ip30_lights_frob(uint32_t, struct trap_frame *);

/*
 * HEART interrupt handling declarations: 64 sources; 5 levels.
 */

struct intrhand *xheart_intrhand[HEART_NINTS];

#ifdef notyet
#define	INTPRI_HEART_4	(INTPRI_CLOCK + 1)
#define	INTPRI_HEART_3	(INTPRI_HEART_4 + 1)
#define	INTPRI_HEART_2	(INTPRI_HEART_3 + 1)
#define	INTPRI_HEART_1	(INTPRI_HEART_2 + 1)
#define	INTPRI_HEART_0	(INTPRI_HEART_1 + 1)
#else
#define	INTPRI_HEART_2	(INTPRI_IPI)
#define	INTPRI_HEART_0	(INTPRI_CLOCK + 1)
#endif
#define	INTPRI_HEART_LEDS	(INTPRI_HEART_0 + 1)

uint64_t xheart_intem[MAXCPUS];
uint64_t xheart_imask[MAXCPUS][NIPLS];

int
xheart_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI4 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI4_HEART)
		return 1;

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

	xbow_node_hub_widget[0/*xaa->xaa_nasid*/] = xaa->xaa_widget;
	xbow_intr_address = 0x80;
	xbow_intr_widget_intr_register = xheart_intr_register;
	xbow_intr_widget_intr_establish = xheart_intr_establish;
	xbow_intr_widget_intr_disestablish = xheart_intr_disestablish;
	xbow_intr_widget_intr_clear = xheart_intr_clear;
	xbow_intr_widget_intr_set = xheart_intr_set;

	/*
	 * Acknowledge and disable all interrupts.
	 */
	heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	*(volatile uint64_t*)(heart + HEART_ISR_CLR) = 0xffffffffffffffff;
	*(volatile uint64_t*)(heart + HEART_IMR(0)) = 0UL;
	*(volatile uint64_t*)(heart + HEART_IMR(1)) = 0UL;
	*(volatile uint64_t*)(heart + HEART_IMR(2)) = 0UL;
	*(volatile uint64_t*)(heart + HEART_IMR(3)) = 0UL;

#ifdef notyet
	set_intr(INTPRI_HEART_4, CR_INT_4, xheart_intr_handler);
	set_intr(INTPRI_HEART_3, CR_INT_3, xheart_intr_handler);
#endif
	set_intr(INTPRI_HEART_2, CR_INT_2, xheart_intr_handler);
#ifdef notyet
	set_intr(INTPRI_HEART_1, CR_INT_1, xheart_intr_handler);
#endif
	set_intr(INTPRI_HEART_0, CR_INT_0, xheart_intr_handler);

	set_intr(INTPRI_HEART_LEDS, CR_INT_5, ip30_lights_frob);

	register_splx_handler(xheart_splx);
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
	int bit;
	u_long cpuid = cpu_number();

	/*
	 * All interrupts will be serviced at hardware level 0,
	 * so the `level' argument can be ignored.
	 */
	for (bit = HEART_INTR_WIDGET_MAX; bit >= HEART_INTR_WIDGET_MIN; bit--)
		if ((xheart_intem[cpuid] & (1UL << bit)) == 0)
			goto found;

	return EINVAL;

found:
	*intrbit = bit;
	return 0;
}

/*
 * Register an interrupt handler for a given source, and enable it.
 */
int
xheart_intr_establish(int (*func)(void *), void *arg, int intrbit,
    int level, const char *name, struct intrhand *ihstore)
{
	struct intrhand *ih;
	int s;
	u_long cpuid = cpu_number();

#ifdef DIAGNOSTIC
	if (intrbit < 0 || intrbit >= HEART_NINTS)
		return EINVAL;
#endif

	/*
	 * HEART interrupts are not supposed to be shared - the interrupt
	 * mask is large enough for all widgets.
	 */
	if (xheart_intrhand[intrbit] != NULL)
		return EEXIST;

	if (ihstore == NULL) {
		ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT);
		if (ih == NULL)
			return ENOMEM;
		ih->ih_flags = IH_ALLOCATED;
	} else {
		ih = ihstore;
		ih->ih_flags = 0;
	}

	ih->ih_next = NULL;
	ih->ih_fun = func;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_irq = intrbit;
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_level);

	s = splhigh();

	xheart_intrhand[intrbit] = ih;

	xheart_intem[cpuid] |= 1UL << intrbit;
	xheart_intr_makemasks();

	splx(s);	/* causes hw mask update */

	return 0;
}

void
xheart_intr_disestablish(int intrbit)
{
	struct intrhand *ih;
	int s;
	u_long cpuid = cpu_number();

#ifdef DIAGNOSTIC
	if (intrbit < 0 || intrbit >= HEART_NINTS)
		return;
#endif

	s = splhigh();

	if ((ih = xheart_intrhand[intrbit]) == NULL) {
		splx(s);
		return;
	}

	xheart_intrhand[intrbit] = NULL;

	xheart_intem[cpuid] &= ~(1UL << intrbit);
	xheart_intr_makemasks();

	splx(s);

	if (ISSET(ih->ih_flags, IH_ALLOCATED))
		free(ih, M_DEVBUF);
}

void
xheart_intr_clear(int intrbit)
{
	*(volatile uint64_t *)PHYS_TO_XKPHYS(HEART_PIU_BASE + HEART_ISR_CLR,
	    CCA_NC) = 1UL << intrbit;
}

void
xheart_intr_set(int intrbit)
{
	*(volatile uint64_t *)PHYS_TO_XKPHYS(HEART_PIU_BASE + HEART_ISR_SET,
	    CCA_NC) = 1UL << intrbit;
}

void
xheart_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");

	if (CPU_IS_PRIMARY(ci))
		xheart_setintrmask(newipl);

	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Heart interrupt handler. Can be registered at any hardware interrupt level.
 */

#define	INTR_FUNCTIONNAME	xheart_intr_handler
#define	MASK_FUNCTIONNAME	xheart_intr_makemasks
#define	INTR_LOCAL_DECLS \
	paddr_t heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC); \
	u_long cpuid = cpu_number();
#define	MASK_LOCAL_DECLS \
	u_long cpuid = cpu_number();
#define	INTR_GETMASKS \
do { \
	isr = *(volatile uint64_t *)(heart + HEART_ISR); \
	imr = *(volatile uint64_t *)(heart + HEART_IMR(cpuid));	\
	switch (hwpend) { \
	case CR_INT_0: \
		isr &= HEART_ISR_LVL0_MASK; \
		bit = HEART_ISR_LVL0_MAX; \
		break; \
	case CR_INT_1: \
		isr &= HEART_ISR_LVL1_MASK; \
		bit = HEART_ISR_LVL1_MAX; \
		break; \
	case CR_INT_2: \
		isr &= HEART_ISR_LVL2_MASK; \
		bit = HEART_ISR_LVL2_MAX; \
		break; \
	case CR_INT_3: \
		isr &= HEART_ISR_LVL3_MASK; \
		bit = HEART_ISR_LVL3_MAX; \
		break; \
	case CR_INT_4: \
		isr &= HEART_ISR_LVL4_MASK; \
		bit = HEART_ISR_LVL4_MAX; \
		break; \
	default: \
		return 0;	/* can't happen */ \
	} \
} while (0)
#define	INTR_MASKPENDING \
	*(volatile uint64_t *)(heart + HEART_IMR(cpuid)) &= ~isr
#define	INTR_IMASK(ipl)		xheart_imask[cpuid][ipl]
#define	INTR_HANDLER(bit)	xheart_intrhand[bit]
#define	INTR_SPURIOUS(bit) \
do { \
	printf("spurious xheart interrupt %d\n", bit); \
} while (0)
#define	INTR_MASKRESTORE \
	*(volatile uint64_t *)(heart + HEART_IMR(cpuid)) = imr
#define	INTR_MASKSIZE	HEART_NINTS

#include <sgi/sgi/intr_template.c>

void
xheart_setintrmask(int level)
{
	paddr_t heart = PHYS_TO_XKPHYS(HEART_PIU_BASE, CCA_NC);
	u_long cpuid = cpu_number();

	*(volatile uint64_t *)(heart + HEART_IMR(cpuid)) =
		xheart_intem[cpuid] & ~xheart_imask[cpuid][level];
}
