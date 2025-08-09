/*	$OpenBSD: bcmstbgpio.c,v 1.1 2025/08/09 14:42:48 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define GIO_DATA(_bank)		(0x04 + (_bank) * 32)
#define GIO_IODIR(_bank)	(0x08 + (_bank) * 32)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct bcmstbgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	u_int			sc_nbanks;

	struct gpio_controller	sc_gc;
};

int bcmstbgpio_match(struct device *, void *, void *);
void bcmstbgpio_attach(struct device *, struct device *, void *);

const struct cfattach	bcmstbgpio_ca = {
	sizeof (struct bcmstbgpio_softc), bcmstbgpio_match, bcmstbgpio_attach
};

struct cfdriver bcmstbgpio_cd = {
	NULL, "bcmstbgpio", DV_DULL
};

void	bcmstbgpio_config_pin(void *, uint32_t *, int);
int	bcmstbgpio_get_pin(void *, uint32_t *);
void	bcmstbgpio_set_pin(void *, uint32_t *, int);

int
bcmstbgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "brcm,brcmstb-gpio");
}

void
bcmstbgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcmstbgpio_softc *sc = (struct bcmstbgpio_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_nbanks = faa->fa_reg[0].size / 32;

	sc->sc_gc.gc_node = faa->fa_node;
	sc->sc_gc.gc_cookie = sc;
	sc->sc_gc.gc_config_pin = bcmstbgpio_config_pin;
	sc->sc_gc.gc_get_pin = bcmstbgpio_get_pin;
	sc->sc_gc.gc_set_pin = bcmstbgpio_set_pin;
	gpio_controller_register(&sc->sc_gc);

	printf("\n");
}

void
bcmstbgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct bcmstbgpio_softc *sc = cookie;
	uint32_t bank = cells[0] / 32;;
	uint32_t pin = cells[0] % 32;

	if (bank >= sc->sc_nbanks)
		return;

	if (config & GPIO_CONFIG_OUTPUT)
		HCLR4(sc, GIO_IODIR(bank), (1U << pin));
	else
		HSET4(sc, GIO_IODIR(bank), (1U << pin));
}

int
bcmstbgpio_get_pin(void *cookie, uint32_t *cells)
{
	struct bcmstbgpio_softc *sc = cookie;
	uint32_t bank = cells[0] / 32;;
	uint32_t pin = cells[0] % 32;
	uint32_t flags = cells[1];
	uint32_t reg;
	int val;

	if (bank >= sc->sc_nbanks)
		return 0;

	reg = HREAD4(sc, GIO_DATA(bank));
	val = !!(reg & (1U << pin));
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	return val;
}

void
bcmstbgpio_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct bcmstbgpio_softc *sc = cookie;
	uint32_t bank = cells[0] / 32;;
	uint32_t pin = cells[0] % 32;
	uint32_t flags = cells[1];

	if (bank >= sc->sc_nbanks)
		return;

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	if (val)
		HSET4(sc, GIO_DATA(bank), (1U << pin));
	else
		HCLR4(sc, GIO_DATA(bank), (1U << pin));
}
