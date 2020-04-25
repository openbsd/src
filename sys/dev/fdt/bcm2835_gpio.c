/*	$OpenBSD: bcm2835_gpio.c,v 1.2 2020/04/25 22:15:00 kettenis Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define GPFSEL(n)		(0x00 + ((n) * 4))
#define  GPFSEL_MASK		0x7
#define  GPFSEL_GPIO_IN		0x0
#define  GPFSEL_GPIO_OUT	0x1
#define  GPFSEL_ALT0		0x4
#define  GPFSEL_ALT1		0x5
#define  GPFSEL_ALT2		0x6
#define  GPFSEL_ALT3		0x7
#define  GPFSEL_ALT4		0x3
#define  GPFSEL_ALT5		0x2
#define GPPUD			0x94
#define  GPPUD_PUD		0x3
#define  GPPUD_PUD_OFF		0x0
#define  GPPUD_PUD_DOWN		0x1
#define  GPPUD_PUD_UP		0x2
#define GPPUDCLK(n)		(0x98 + ((n) * 4))
#define GPPULL(n)		(0xe4 + ((n) * 4))
#define  GPPULL_MASK		0x3

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct bcmgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	void	(*sc_config_pull)(struct bcmgpio_softc *, int, int);
};

int	bcmgpio_match(struct device *, void *, void *);
void	bcmgpio_attach(struct device *, struct device *, void *);

struct cfattach	bcmgpio_ca = {
	sizeof (struct bcmgpio_softc), bcmgpio_match, bcmgpio_attach
};

struct cfdriver bcmgpio_cd = {
	NULL, "bcmgpio", DV_DULL
};

void	bcm2711_config_pull(struct bcmgpio_softc *, int, int);
void	bcm2835_config_pull(struct bcmgpio_softc *, int, int);
int	bcmgpio_pinctrl(uint32_t, void *);

int
bcmgpio_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "brcm,bcm2711-gpio") ||
	    OF_is_compatible(faa->fa_node, "brcm,bcm2835-gpio"));
}

void
bcmgpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct bcmgpio_softc *sc = (struct bcmgpio_softc *)self;
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

	printf("\n");

	if (OF_is_compatible(faa->fa_node, "brcm,bcm2711-gpio"))
	    sc->sc_config_pull = bcm2711_config_pull;
	else
	    sc->sc_config_pull = bcm2835_config_pull;

	pinctrl_register(faa->fa_node, bcmgpio_pinctrl, sc);
}

void
bcmgpio_config_func(struct bcmgpio_softc *sc, int pin, int func)
{
	int reg = (pin / 10);
	int shift = (pin % 10) * 3;
	uint32_t val;

	val = HREAD4(sc, GPFSEL(reg));
	val &= ~(GPFSEL_MASK << shift);
	HWRITE4(sc, GPFSEL(reg), val);
	val |= ((func & GPFSEL_MASK) << shift);
	HWRITE4(sc, GPFSEL(reg), val);
}

void
bcm2711_config_pull(struct bcmgpio_softc *sc, int pin, int pull)
{
	int reg = (pin / 16);
	int shift = (pin % 16) * 2;
	uint32_t val;

	val = HREAD4(sc, GPPULL(reg));
	val &= ~(GPPULL_MASK << shift);
	pull = ((pull & 1) << 1) | ((pull & 2) >> 1);
	val |= (pull << shift);
	HWRITE4(sc, GPPULL(reg), val);
}

void
bcm2835_config_pull(struct bcmgpio_softc *sc, int pin, int pull)
{
	int reg = (pin / 32);
	int shift = (pin % 32);

	HWRITE4(sc, GPPUD, pull & GPPUD_PUD);
	delay(1);
	HWRITE4(sc, GPPUDCLK(reg), 1 << shift);
	delay(1);
	HWRITE4(sc, GPPUDCLK(reg), 0);
}

int
bcmgpio_pinctrl(uint32_t phandle, void *cookie)
{
	struct bcmgpio_softc *sc = cookie;
	uint32_t *pins, *pull = NULL;
	int len, plen = 0;
	int node, i;
	int func;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "brcm,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getpropintarray(node, "brcm,pins", pins, len) != len)
		goto fail;
	func = OF_getpropint(node, "brcm,function", -1);

	plen = OF_getproplen(node, "brcm,pull");
	if (plen > 0) {
		pull = malloc(len, M_TEMP, M_WAITOK);
		if (OF_getpropintarray(node, "brcm,pull", pull, plen) != plen)
			goto fail;
	}

	for (i = 0; i < len / sizeof(uint32_t); i++) {
		bcmgpio_config_func(sc, pins[i], func);
		if (plen > 0 && i < plen / sizeof(uint32_t))
			sc->sc_config_pull(sc, pins[i], pull[i]);
	}

	free(pull, M_TEMP, plen);
	free(pins, M_TEMP, len);
	return 0;

fail:
	free(pull, M_TEMP, plen);
	free(pins, M_TEMP, len);
	return -1;
}
