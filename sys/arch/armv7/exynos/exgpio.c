/* $OpenBSD: exgpio.c,v 1.2 2015/05/27 00:06:14 jsg Exp $ */
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

#include "fdt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#if NFDT > 0
#include <machine/fdt.h>
#endif
#include <machine/intr.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/exynos/exgpiovar.h>

/* Exynos5 registers */
#define GPIO_BANK_SIZE		0x20
#define GPIO_BANK(x)		(GPIO_BANK_SIZE * ((x) / 8))
#define GPIO_CON(x)		(GPIO_BANK(x) + 0x00)
#define GPIO_DAT(x)		(GPIO_BANK(x) + 0x04)
#define GPIO_PULL(x)		(GPIO_BANK(x) + 0x08)
#define GPIO_DRV(x)		(GPIO_BANK(x) + 0x0c)
#define GPIO_PDN_CON(x)		(GPIO_BANK(x) + 0x10)
#define GPIO_PDN_PULL(x)	(GPIO_BANK(x) + 0x14)

/* bits and bytes */
#define GPIO_PIN(x)		((x) % 8)
#define GPIO_CON_INPUT(x)	(0x0 << (GPIO_PIN(x) << 2))
#define GPIO_CON_OUTPUT(x)	(0x1 << (GPIO_PIN(x) << 2))
#define GPIO_CON_IRQ(x)		(0xf << (GPIO_PIN(x) << 2))
#define GPIO_CON_MASK(x)	(0xf << (GPIO_PIN(x) << 2))
#define GPIO_DAT_SET(x)		(0x1 << (GPIO_PIN(x) << 0))
#define GPIO_DAT_MASK(x)	(0x1 << (GPIO_PIN(x) << 0))
#define GPIO_PULL_NONE(x)	(0x0 << (GPIO_PIN(x) << 1))
#define GPIO_PULL_DOWN(x)	(0x1 << (GPIO_PIN(x) << 1))
#define GPIO_PULL_UP(x)		(0x3 << (GPIO_PIN(x) << 1))
#define GPIO_PULL_MASK(x)	(0x3 << (GPIO_PIN(x) << 1))
#define GPIO_DRV_1X(x)		(0x0 << (GPIO_PIN(x) << 1))
#define GPIO_DRV_2X(x)		(0x1 << (GPIO_PIN(x) << 1))
#define GPIO_DRV_3X(x)		(0x2 << (GPIO_PIN(x) << 1))
#define GPIO_DRV_4X(x)		(0x3 << (GPIO_PIN(x) << 1))
#define GPIO_DRV_MASK(x)	(0x3 << (GPIO_PIN(x) << 1))

#define GPIO_PINS_PER_BANK	8

struct exgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_ngpio;
	unsigned int (*sc_get_bit)(struct exgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_bit)(struct exgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_clear_bit)(struct exgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_dir)(struct exgpio_softc *sc,
	    unsigned int gpio, unsigned int dir);
};

int exgpio_match(struct device *parent, void *v, void *aux);
void exgpio_attach(struct device *parent, struct device *self, void *args);

struct exgpio_softc *exgpio_pin_to_inst(unsigned int);
unsigned int exgpio_pin_to_offset(unsigned int);
unsigned int exgpio_v5_get_bit(struct exgpio_softc *, unsigned int);
void exgpio_v5_set_bit(struct exgpio_softc *, unsigned int);
void exgpio_v5_clear_bit(struct exgpio_softc *, unsigned int);
void exgpio_v5_set_dir(struct exgpio_softc *, unsigned int, unsigned int);
unsigned int exgpio_v5_get_dir(struct exgpio_softc *, unsigned int);


struct cfattach	exgpio_ca = {
	sizeof (struct exgpio_softc), NULL, exgpio_attach
};
struct cfattach	exgpio_fdt_ca = {
	sizeof (struct exgpio_softc), exgpio_match, exgpio_attach
};

struct cfdriver exgpio_cd = {
	NULL, "exgpio", DV_DULL
};

int
exgpio_match(struct device *parent, void *v, void *aux)
{
#if NFDT > 0
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,exynos5250-pinctrl", aa->aa_node))
		return 1;
#endif

	return 0;
}

void
exgpio_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct exgpio_softc *sc = (struct exgpio_softc *) self;
	struct armv7mem mem;

	sc->sc_iot = aa->aa_iot;
#if NFDT > 0
	if (aa->aa_node) {
		struct fdt_memory fdtmem;
		if (fdt_get_memory_address(aa->aa_node, 0, &fdtmem))
			panic("%s: could not extract memory data from FDT",
			    __func__);
		mem.addr = fdtmem.addr;
		mem.size = fdtmem.size;
	} else
#endif
	{
		mem.addr = aa->aa_dev->mem[0].addr;
		mem.size = aa->aa_dev->mem[0].size;
	}
	if (bus_space_map(sc->sc_iot, mem.addr, mem.size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	sc->sc_ngpio = (mem.size / GPIO_BANK_SIZE) * GPIO_PINS_PER_BANK;

	sc->sc_get_bit  = exgpio_v5_get_bit;
	sc->sc_set_bit = exgpio_v5_set_bit;
	sc->sc_clear_bit = exgpio_v5_clear_bit;
	sc->sc_set_dir = exgpio_v5_set_dir;

	printf("\n");

	/* XXX - IRQ */
	/* XXX - SYSCONFIG */
	/* XXX - CTRL */
	/* XXX - DEBOUNCE */
}

struct exgpio_softc *
exgpio_pin_to_inst(unsigned int gpio)
{
	int i;

	for (i = 0; exgpio_cd.cd_devs[i] != NULL; i++)
	{
		struct exgpio_softc *sc = exgpio_cd.cd_devs[i];
		if (gpio < sc->sc_ngpio)
			return sc;
		else
			gpio -= sc->sc_ngpio;
	}

	return NULL;
}

unsigned int
exgpio_pin_to_offset(unsigned int gpio)
{
	int i;

	for (i = 0; exgpio_cd.cd_devs[i] != NULL; i++)
	{
		struct exgpio_softc *sc = exgpio_cd.cd_devs[i];
		if (gpio < sc->sc_ngpio)
			return gpio;
		else
			gpio -= sc->sc_ngpio;
	}

	return 0;
}

unsigned int
exgpio_get_bit(unsigned int gpio)
{
	struct exgpio_softc *sc = exgpio_pin_to_inst(gpio);

	return sc->sc_get_bit(sc, gpio);
}

void
exgpio_set_bit(unsigned int gpio)
{
	struct exgpio_softc *sc = exgpio_pin_to_inst(gpio);

	sc->sc_set_bit(sc, gpio);
}

void
exgpio_clear_bit(unsigned int gpio)
{
	struct exgpio_softc *sc = exgpio_pin_to_inst(gpio);

	sc->sc_clear_bit(sc, gpio);
}
void
exgpio_set_dir(unsigned int gpio, unsigned int dir)
{
	struct exgpio_softc *sc = exgpio_pin_to_inst(gpio);

	sc->sc_set_dir(sc, gpio, dir);
}

unsigned int
exgpio_v5_get_bit(struct exgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	gpio = exgpio_pin_to_offset(gpio);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DAT(gpio));

	return !!(val & GPIO_DAT_SET(gpio));
}

void
exgpio_v5_set_bit(struct exgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	gpio = exgpio_pin_to_offset(gpio);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DAT(gpio));

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_DAT(gpio),
		val | GPIO_DAT_SET(gpio));
}

void
exgpio_v5_clear_bit(struct exgpio_softc *sc, unsigned int gpio)
{
	u_int32_t val;

	gpio = exgpio_pin_to_offset(gpio);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DAT(gpio));

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_DAT(gpio),
		val & ~GPIO_DAT_MASK(gpio));
}

void
exgpio_v5_set_dir(struct exgpio_softc *sc, unsigned int gpio, unsigned int dir)
{
	int s;
	u_int32_t val;

	gpio = exgpio_pin_to_offset(gpio);
	s = splhigh();

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_CON(gpio));
	val &= ~GPIO_CON_OUTPUT(gpio);
	if (dir == EXGPIO_DIR_OUT)
		val |= GPIO_CON_OUTPUT(gpio);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_CON(gpio), val);

	splx(s);
}

unsigned int
exgpio_v5_get_dir(struct exgpio_softc *sc, unsigned int gpio)
{
	int s;
	u_int32_t val;

	gpio = exgpio_pin_to_offset(gpio);
	s = splhigh();

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_CON(gpio));
	if (val & GPIO_CON_OUTPUT(gpio))
		val = EXGPIO_DIR_OUT;
	else
		val = EXGPIO_DIR_IN;

	splx(s);
	return val;
}
