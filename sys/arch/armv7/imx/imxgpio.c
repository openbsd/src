/* $OpenBSD: imxgpio.c,v 1.6 2015/05/08 04:47:27 jsg Exp $ */
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
#include <machine/intr.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxgpiovar.h>

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
};

#define GPIO_PIN_TO_INST(x)	((x) >> 5)
#define GPIO_PIN_TO_OFFSET(x)	((x) & 0x1f)

int imxgpio_match(struct device *parent, void *v, void *aux);
void imxgpio_attach(struct device *parent, struct device *self, void *args);
void imxgpio_recalc_interrupts(struct imxgpio_softc *sc);
int imxgpio_irq(void *);
int imxgpio_irq_dummy(void *);

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
imxgpio_match(struct device *parent, void *v, void *aux)
{
	switch (board_id) {
	case BOARD_ID_IMX6_CUBOXI:
	case BOARD_ID_IMX6_HUMMINGBOARD:
	case BOARD_ID_IMX6_NOVENA:
	case BOARD_ID_IMX6_PHYFLEX:
	case BOARD_ID_IMX6_SABRELITE:
	case BOARD_ID_IMX6_SABRESD:
	case BOARD_ID_IMX6_UDOO:
	case BOARD_ID_IMX6_UTILITE:
	case BOARD_ID_IMX6_WANDBOARD:
		break; /* continue trying */
	default:
		return 0; /* unknown */
	}
	return (1);
}

void
imxgpio_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct imxgpio_softc *sc = (struct imxgpio_softc *) self;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("imxgpio_attach: bus_space_map failed!");


	switch (board_id) {
		case BOARD_ID_IMX6_CUBOXI:
		case BOARD_ID_IMX6_HUMMINGBOARD:
		case BOARD_ID_IMX6_NOVENA:
		case BOARD_ID_IMX6_PHYFLEX:
		case BOARD_ID_IMX6_SABRELITE:
		case BOARD_ID_IMX6_SABRESD:
		case BOARD_ID_IMX6_UDOO:
		case BOARD_ID_IMX6_UTILITE:
		case BOARD_ID_IMX6_WANDBOARD:
			sc->sc_get_bit  = imxgpio_v6_get_bit;
			sc->sc_set_bit = imxgpio_v6_set_bit;
			sc->sc_clear_bit = imxgpio_v6_clear_bit;
			sc->sc_set_dir = imxgpio_v6_set_dir;
			break;
	}

	printf("\n");

	/* XXX - IRQ */
	/* XXX - SYSCONFIG */
	/* XXX - CTRL */
	/* XXX - DEBOUNCE */
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
