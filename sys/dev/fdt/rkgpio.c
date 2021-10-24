/*	$OpenBSD: rkgpio.c,v 1.7 2021/10/24 17:52:26 mpi Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2019 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define GPIO_SWPORTA_DR		0x0000
#define GPIO_SWPORTA_DDR	0x0004
#define GPIO_INTEN		0x0030
#define GPIO_INTMASK		0x0034
#define GPIO_INTTYPE_LEVEL	0x0038
#define GPIO_INT_POLARITY	0x003c
#define GPIO_INT_STATUS		0x0040
#define GPIO_INT_RAWSTATUS	0x0044
#define GPIO_DEBOUNCE		0x0048
#define GPIO_PORTS_EOI		0x004c
#define GPIO_EXT_PORTA		0x0050

#define GPIO_NUM_PINS		32

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct intrhand {
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	int ih_level;			/* GPIO level */
	struct evcount	ih_count;
	char *ih_name;
	void *ih_sc;
};

struct rkgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	void			*sc_ih;
	int			sc_ipl;
	int			sc_irq;
	struct intrhand		*sc_handlers[GPIO_NUM_PINS];
	struct interrupt_controller sc_ic;

	struct gpio_controller	sc_gc;
};

int rkgpio_match(struct device *, void *, void *);
void rkgpio_attach(struct device *, struct device *, void *);

const struct cfattach	rkgpio_ca = {
	sizeof (struct rkgpio_softc), rkgpio_match, rkgpio_attach
};

struct cfdriver rkgpio_cd = {
	NULL, "rkgpio", DV_DULL
};

void	rkgpio_config_pin(void *, uint32_t *, int);
int	rkgpio_get_pin(void *, uint32_t *);
void	rkgpio_set_pin(void *, uint32_t *, int);

int	rkgpio_intr(void *);
void	*rkgpio_intr_establish(void *, int *, int, struct cpu_info *,
	    int (*)(void *), void *, char *);
void	rkgpio_intr_disestablish(void *);
void	rkgpio_recalc_ipl(struct rkgpio_softc *);
void	rkgpio_intr_enable(void *);
void	rkgpio_intr_disable(void *);
void	rkgpio_intr_barrier(void *);

int
rkgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,gpio-bank");
}

void
rkgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkgpio_softc *sc = (struct rkgpio_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	sc->sc_iot = faa->fa_iot;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_gc.gc_node = faa->fa_node;
	sc->sc_gc.gc_cookie = sc;
	sc->sc_gc.gc_config_pin = rkgpio_config_pin;
	sc->sc_gc.gc_get_pin = rkgpio_get_pin;
	sc->sc_gc.gc_set_pin = rkgpio_set_pin;
	gpio_controller_register(&sc->sc_gc);

	sc->sc_ipl = IPL_NONE;
	HWRITE4(sc, GPIO_INTMASK, ~0);
	HWRITE4(sc, GPIO_INTEN, ~0);

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish = rkgpio_intr_establish;
	sc->sc_ic.ic_disestablish = rkgpio_intr_disestablish;
	sc->sc_ic.ic_enable = rkgpio_intr_enable;
	sc->sc_ic.ic_disable = rkgpio_intr_disable;
	sc->sc_ic.ic_barrier = rkgpio_intr_barrier;
	fdt_intr_register(&sc->sc_ic);

	printf("\n");
}

void
rkgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct rkgpio_softc *sc = cookie;
	uint32_t pin = cells[0];

	if (pin >= GPIO_NUM_PINS)
		return;

	if (config & GPIO_CONFIG_OUTPUT)
		HSET4(sc, GPIO_SWPORTA_DDR, (1 << pin));
	else
		HCLR4(sc, GPIO_SWPORTA_DDR, (1 << pin));
}

int
rkgpio_get_pin(void *cookie, uint32_t *cells)
{
	struct rkgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	uint32_t reg;
	int val;

	if (pin >= GPIO_NUM_PINS)
		return 0;

	reg = HREAD4(sc, GPIO_EXT_PORTA);
	val = (reg >> pin) & 1;
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	return val;
}

void
rkgpio_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct rkgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];

	if (pin >= GPIO_NUM_PINS)
		return;

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	if (val)
		HSET4(sc, GPIO_SWPORTA_DR, (1 << pin));
	else
		HCLR4(sc, GPIO_SWPORTA_DR, (1 << pin));
}

int
rkgpio_intr(void *cookie)
{
	struct rkgpio_softc	*sc = (struct rkgpio_softc *)cookie;
	struct intrhand		*ih;
	uint32_t		 status, pending;
	int			 pin, s;

	status = HREAD4(sc, GPIO_INT_STATUS);
	pending = status;

	while (pending) {
		pin = ffs(pending) - 1;

		if ((ih = sc->sc_handlers[pin]) != NULL) {
			s = splraise(ih->ih_ipl);
			if (ih->ih_func(ih->ih_arg))
				ih->ih_count.ec_count++;
			splx(s);
		}

		pending &= ~(1 << pin);
	}

	HWRITE4(sc, GPIO_PORTS_EOI, status);

	return 1;
}

void *
rkgpio_intr_establish(void *cookie, int *cells, int ipl,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct rkgpio_softc	*sc = (struct rkgpio_softc *)cookie;
	struct intrhand		*ih;
	int			 irqno = cells[0];
	int			 level = cells[1];
	int			 s;

	if (irqno < 0 || irqno >= GPIO_NUM_PINS)
		panic("%s: bogus irqnumber %d: %s", __func__,
		     irqno, name);

	if (sc->sc_handlers[irqno] != NULL)
		panic("%s: irqnumber %d reused: %s", __func__,
		     irqno, name);

	if (ci != NULL && !CPU_IS_PRIMARY(ci))
		return NULL;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	ih->ih_irq = irqno;
	ih->ih_name = name;
	ih->ih_level = level;
	ih->ih_sc = sc;

	s = splhigh();

	sc->sc_handlers[irqno] = ih;

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_INTC
	printf("%s: irq %d ipl %d [%s]\n", __func__, ih->ih_irq, ih->ih_ipl,
	    ih->ih_name);
#endif

	rkgpio_recalc_ipl(sc);

	switch (level) {
	case 1: /* rising */
		HSET4(sc, GPIO_INTTYPE_LEVEL, 1 << irqno);
		HSET4(sc, GPIO_INT_POLARITY, 1 << irqno);
		break;
	case 2: /* falling */
		HSET4(sc, GPIO_INTTYPE_LEVEL, 1 << irqno);
		HCLR4(sc, GPIO_INT_POLARITY, 1 << irqno);
		break;
	case 4: /* high */
		HCLR4(sc, GPIO_INTTYPE_LEVEL, 1 << irqno);
		HSET4(sc, GPIO_INT_POLARITY, 1 << irqno);
		break;
	case 8: /* low */
		HCLR4(sc, GPIO_INTTYPE_LEVEL, 1 << irqno);
		HCLR4(sc, GPIO_INT_POLARITY, 1 << irqno);
		break;
	default:
		panic("%s: unsupported trigger type", __func__);
	}

	HCLR4(sc, GPIO_SWPORTA_DDR, 1 << irqno);
	HCLR4(sc, GPIO_INTMASK, 1 << irqno);

	splx(s);
	return (ih);
}

void
rkgpio_intr_disestablish(void *cookie)
{
	struct intrhand		*ih = cookie;
	struct rkgpio_softc	*sc = ih->ih_sc;
	int			 s;

	s = splhigh();

#ifdef DEBUG_INTC
	printf("%s: irq %d ipl %d [%s]\n", __func__, ih->ih_irq, ih->ih_ipl,
	    ih->ih_name);
#endif

	HSET4(sc, GPIO_INTMASK, 1 << ih->ih_irq);

	sc->sc_handlers[ih->ih_irq] = NULL;
	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);
	free(ih, M_DEVBUF, sizeof(*ih));

	rkgpio_recalc_ipl(sc);

	splx(s);
}

void
rkgpio_recalc_ipl(struct rkgpio_softc *sc)
{
	struct intrhand		*ih;
	int			 pin;
	int			 max = IPL_NONE;
	int			 min = IPL_HIGH;

	for (pin = 0; pin < GPIO_NUM_PINS; pin++) {
		ih = sc->sc_handlers[pin];
		if (ih == NULL)
			continue;

		if (ih->ih_ipl > max)
			max = ih->ih_ipl;

		if (ih->ih_ipl < min)
			min = ih->ih_ipl;
	}

	if (max == IPL_NONE)
		min = IPL_NONE;

	if (sc->sc_ipl != max) {
		sc->sc_ipl = max;

		if (sc->sc_ih != NULL)
			fdt_intr_disestablish(sc->sc_ih);

		if (sc->sc_ipl != IPL_NONE)
			sc->sc_ih = fdt_intr_establish(sc->sc_node,
			    sc->sc_ipl, rkgpio_intr, sc, sc->sc_dev.dv_xname);
	}
}

void
rkgpio_intr_enable(void *cookie)
{
	struct intrhand		*ih = cookie;
	struct rkgpio_softc	*sc = ih->ih_sc;
	int			 s;

	s = splhigh();
	HCLR4(sc, GPIO_INTMASK, 1 << ih->ih_irq);
	splx(s);
}

void
rkgpio_intr_disable(void *cookie)
{
	struct intrhand		*ih = cookie;
	struct rkgpio_softc	*sc = ih->ih_sc;
	int			 s;

	s = splhigh();
	HSET4(sc, GPIO_INTMASK, 1 << ih->ih_irq);
	splx(s);
}

void
rkgpio_intr_barrier(void *cookie)
{
	struct intrhand		*ih = cookie;
	struct rkgpio_softc	*sc = ih->ih_sc;

	intr_barrier(sc->sc_ih);
}
