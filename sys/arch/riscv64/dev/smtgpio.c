/*	$OpenBSD: smtgpio.c,v 1.1 2026/04/03 12:47:06 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#define GPIO_PLR	0x0000
#define GPIO_PDR	0x000c
#define GPIO_PSR	0x0018
#define GPIO_PCR	0x0024

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct smtgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct gpio_controller	sc_gc;
};

int smtgpio_match(struct device *, void *, void *);
void smtgpio_attach(struct device *, struct device *, void *);

const struct cfattach	smtgpio_ca = {
	sizeof (struct smtgpio_softc), smtgpio_match, smtgpio_attach
};

struct cfdriver smtgpio_cd = {
	NULL, "smtgpio", DV_DULL
};

void	smtgpio_config_pin(void *, uint32_t *, int);
int	smtgpio_get_pin(void *, uint32_t *);
void	smtgpio_set_pin(void *, uint32_t *, int);

int
smtgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-gpio");
}

void
smtgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtgpio_softc *sc = (struct smtgpio_softc *)self;
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
	sc->sc_gc.gc_config_pin = smtgpio_config_pin;
	sc->sc_gc.gc_get_pin = smtgpio_get_pin;
	sc->sc_gc.gc_set_pin = smtgpio_set_pin;
	gpio_controller_register(&sc->sc_gc);

	printf("\n");
}

bus_size_t
smtgpio_bank_offset(uint32_t bank)
{
	switch (bank) {
	case 0:
		return 0x0000;
	case 1:
		return 0x0004;
	case 2:
		return 0x0008;
	case 3:
		return 0x0100;
	}

	return (bus_size_t)-1;
}

void
smtgpio_config_pin(void *cookie, uint32_t *cells, int config)
{
	struct smtgpio_softc *sc = cookie;
	uint32_t bank = cells[0];
	uint32_t pin = cells[1];
	bus_size_t offset = smtgpio_bank_offset(bank);

	if (offset == (bus_size_t)-1 || pin >= 32)
		return;

	if (config & GPIO_CONFIG_OUTPUT)
		HSET4(sc, offset + GPIO_PDR, (1U << pin));
	else
		HCLR4(sc, offset + GPIO_PDR, (1U << pin));
}

int
smtgpio_get_pin(void *cookie, uint32_t *cells)
{
	struct smtgpio_softc *sc = cookie;
	uint32_t bank = cells[0];
	uint32_t pin = cells[1];
	uint32_t flags = cells[2];
	bus_size_t offset = smtgpio_bank_offset(bank);
	uint32_t reg;
	int val;

	if (offset == (bus_size_t)-1 || pin >= 32)
		return 0;

	reg = HREAD4(sc, offset + GPIO_PLR);
	val = (reg >> pin) & 1;
	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	return val;
}

void
smtgpio_set_pin(void *cookie, uint32_t *cells, int val)
{
	struct smtgpio_softc *sc = cookie;
	uint32_t bank = cells[0];
	uint32_t pin = cells[1];
	uint32_t flags = cells[2];
	bus_size_t offset = smtgpio_bank_offset(bank);

	if (offset == (bus_size_t)-1 || pin >= 32)
		return;

	if (flags & GPIO_ACTIVE_LOW)
		val = !val;
	if (val)
		HWRITE4(sc, offset + GPIO_PSR, (1U << pin));
	else
		HWRITE4(sc, offset + GPIO_PCR, (1U << pin));
}
