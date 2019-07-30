/*	$OpenBSD: rkgpio.c,v 1.1 2017/05/06 18:25:43 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#define GPIO_SWPORTA_DR		0x0000
#define GPIO_SWPORTA_DDR	0x0004
#define GPIO_EXT_PORTA		0x0050

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct rkgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct gpio_controller	sc_gc;
};

int rkgpio_match(struct device *, void *, void *);
void rkgpio_attach(struct device *, struct device *, void *);

struct cfattach	rkgpio_ca = {
	sizeof (struct rkgpio_softc), rkgpio_match, rkgpio_attach
};

struct cfdriver rkgpio_cd = {
	NULL, "rkgpio", DV_DULL
};

void	rkgpio_config_pin(void *, uint32_t *, int);
int	rkgpio_get_pin(void *, uint32_t *);
void	rkgpio_set_pin(void *, uint32_t *, int);

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

	printf("\n");
}

void
rkgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct rkgpio_softc *sc = cookie;
	uint32_t pin = cells[0];

	if (pin > 32)
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

	if (pin > 32)
		return 0;

	reg = HREAD4(sc, GPIO_EXT_PORTA);
	reg &= (1 << pin);
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

	if (pin > 32)
		return;

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	if (val)
		HSET4(sc, GPIO_SWPORTA_DR, (1 << pin));
	else
		HCLR4(sc, GPIO_SWPORTA_DR, (1 << pin));
}
