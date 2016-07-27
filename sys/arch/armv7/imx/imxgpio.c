/* $OpenBSD: imxgpio.c,v 1.10 2016/07/27 11:45:02 patrick Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxgpiovar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

/* iMX6 registers */
#define GPIO_DR			0x00
#define GPIO_GDIR		0x04
#define GPIO_PSR		0x08
#define GPIO_ICR1		0x0C
#define GPIO_ICR2		0x10
#define GPIO_IMR		0x14
#define GPIO_ISR		0x18
#define GPIO_EDGE_SEL		0x1C

#define GPIO_NUM_PINS		32

struct intrhand {
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	int ih_gpio;			/* gpio pin */
	struct evcount	ih_count;
	char *ih_name;
};

struct imxgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void			*sc_ih_h;
	void			*sc_ih_l;
	int 			sc_max_il;
	int 			sc_min_il;
	int			sc_irq;
	struct intrhand		*sc_handlers[GPIO_NUM_PINS];
	unsigned int (*sc_get_bit)(struct imxgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_bit)(struct imxgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_clear_bit)(struct imxgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_dir)(struct imxgpio_softc *sc,
	    unsigned int gpio, unsigned int dir);
	struct gpio_controller sc_gc;
};

#define GPIO_PIN_TO_INST(x)	((x) >> 5)
#define GPIO_PIN_TO_OFFSET(x)	((x) & 0x1f)

int imxgpio_match(struct device *, void *, void *);
void imxgpio_attach(struct device *, struct device *, void *);

void imxgpio_config_pin(void *, uint32_t *, int);
int imxgpio_get_pin(void *, uint32_t *);
void imxgpio_set_pin(void *, uint32_t *, int);

unsigned int imxgpio_v6_get_bit(struct imxgpio_softc *, unsigned int);
void imxgpio_v6_set_bit(struct imxgpio_softc *, unsigned int);
void imxgpio_v6_clear_bit(struct imxgpio_softc *, unsigned int);
void imxgpio_v6_set_dir(struct imxgpio_softc *, unsigned int, unsigned int);
unsigned int imxgpio_v6_get_dir(struct imxgpio_softc *, unsigned int);


struct cfattach	imxgpio_ca = {
	sizeof (struct imxgpio_softc), imxgpio_match, imxgpio_attach
};

struct cfdriver imxgpio_cd = {
	NULL, "imxgpio", DV_DULL
};

int
imxgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "fsl,imx35-gpio");
}

void
imxgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxgpio_softc *sc = (struct imxgpio_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("imxgpio_attach: bus_space_map failed!");

	sc->sc_gc.gc_node = faa->fa_node;
	sc->sc_gc.gc_cookie = sc;
	sc->sc_gc.gc_config_pin = imxgpio_config_pin;
	sc->sc_gc.gc_get_pin = imxgpio_get_pin;
	sc->sc_gc.gc_set_pin = imxgpio_set_pin;
	gpio_controller_register(&sc->sc_gc);

	sc->sc_get_bit  = imxgpio_v6_get_bit;
	sc->sc_set_bit = imxgpio_v6_set_bit;
	sc->sc_clear_bit = imxgpio_v6_clear_bit;
	sc->sc_set_dir = imxgpio_v6_set_dir;

	printf("\n");

	/* XXX - IRQ */
	/* XXX - SYSCONFIG */
	/* XXX - CTRL */
	/* XXX - DEBOUNCE */
}

void
imxgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct imxgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t val;

	if (pin >= GPIO_NUM_PINS)
		return;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_GDIR);
	if (config & GPIO_CONFIG_OUTPUT)
		val |= 1 << pin;
	else
		val &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_GDIR, val);
}

int
imxgpio_get_pin(void *cookie, uint32_t *cells)
{
	struct imxgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	uint32_t reg;
	int val;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DR);
	reg &= (1 << pin);
	val = (reg >> pin) & 1;
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;;
	return val;
}

void
imxgpio_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct imxgpio_softc *sc = cookie;
	uint32_t pin = cells[0];
	uint32_t flags = cells[1];
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DR);
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	if (val)
		reg |= (1 << pin);
	else
		reg &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_DR, reg);
}

unsigned int
imxgpio_get_bit(unsigned int gpio)
{
	struct imxgpio_softc *sc = imxgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	return sc->sc_get_bit(sc, gpio);
	
}

void
imxgpio_set_bit(unsigned int gpio)
{
	struct imxgpio_softc *sc = imxgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_set_bit(sc, gpio);
}

void
imxgpio_clear_bit(unsigned int gpio)
{
	struct imxgpio_softc *sc = imxgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_clear_bit(sc, gpio);
}
void
imxgpio_set_dir(unsigned int gpio, unsigned int dir)
{
	struct imxgpio_softc *sc = imxgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_set_dir(sc, gpio, dir);
}

unsigned int
imxgpio_v6_get_bit(struct imxgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DR);

	return (val >> GPIO_PIN_TO_OFFSET(gpio)) & 0x1;
}

void
imxgpio_v6_set_bit(struct imxgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DR);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_DR,
		val | (1 << GPIO_PIN_TO_OFFSET(gpio)));
}

void
imxgpio_v6_clear_bit(struct imxgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DR);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_DR,
		val & ~(1 << GPIO_PIN_TO_OFFSET(gpio)));
}

void
imxgpio_v6_set_dir(struct imxgpio_softc *sc, unsigned int gpio, unsigned int dir)
{
	int s;
	u_int32_t val;

	s = splhigh();

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_GDIR);
	if (dir == IMXGPIO_DIR_OUT)
		val |= 1 << GPIO_PIN_TO_OFFSET(gpio);
	else
		val &= ~(1 << GPIO_PIN_TO_OFFSET(gpio));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_GDIR, val);

	splx(s);
}

unsigned int
imxgpio_v6_get_dir(struct imxgpio_softc *sc, unsigned int gpio)
{
	int s;
	u_int32_t val;

	s = splhigh();

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_GDIR);
	if (val & (1 << GPIO_PIN_TO_OFFSET(gpio)))
		val = IMXGPIO_DIR_OUT;
	else
		val = IMXGPIO_DIR_IN;

	splx(s);
	return val;
}
