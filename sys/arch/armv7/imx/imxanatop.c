/*
 * Copyright (c) 2016 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct imxanatop_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct imxanatop_regulator {
	struct imxanatop_softc *ir_sc;

	uint32_t ir_reg_offset;
	uint32_t ir_vol_bit_shift;
	uint32_t ir_vol_bit_width;
	uint32_t ir_min_bit_val;
	uint32_t ir_min_voltage;
	uint32_t ir_max_voltage;

	uint32_t ir_delay_reg_offset;
	uint32_t ir_delay_bit_shift;
	uint32_t ir_delay_bit_width;

	struct regulator_device ir_rd;
};

int	imxanatop_match(struct device *, void *, void *);
void	imxanatop_attach(struct device *, struct device *, void *);

struct cfattach imxanatop_ca = {
	sizeof(struct imxanatop_softc), imxanatop_match, imxanatop_attach
};

struct cfdriver imxanatop_cd = {
	NULL, "imxanatop", DV_DULL
};

void	imxanatop_attach_regulator(struct imxanatop_softc *, int);
uint32_t imxanatop_get_voltage(void *);
int	imxanatop_set_voltage(void *, uint32_t);

int
imxanatop_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "fsl,imx6q-anatop"))
		return 10;	/* Must beat simplebus(4) and syscon(4). */

	return 0;
}

void
imxanatop_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxanatop_softc *sc = (struct imxanatop_softc *)self;
	struct fdt_attach_args *faa = aux;
	int node;

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

	regmap_register(faa->fa_node, sc->sc_iot, sc->sc_ioh,
	    faa->fa_reg[0].size);

	printf("\n");

	for (node = OF_child(faa->fa_node); node; node = OF_peer(node))
		if (OF_is_compatible(node, "fsl,anatop-regulator"))
			imxanatop_attach_regulator(sc, node);
}

void
imxanatop_attach_regulator(struct imxanatop_softc *sc, int node)
{
	struct imxanatop_regulator *ir;

	ir = malloc(sizeof(*ir), M_DEVBUF, M_WAITOK | M_ZERO);
	ir->ir_sc = sc;

	ir->ir_reg_offset = OF_getpropint(node, "anatop-reg-offset", -1);
	ir->ir_vol_bit_shift = OF_getpropint(node, "anatop-vol-bit-shift", -1);
	ir->ir_vol_bit_width = OF_getpropint(node, "anatop-vol-bit-width", -1);
	ir->ir_min_bit_val = OF_getpropint(node, "anatop-min-bit-val", -1);
	ir->ir_min_voltage = OF_getpropint(node, "anatop-min-voltage", -1);
	ir->ir_max_voltage = OF_getpropint(node, "anatop-max-voltage", -1);
	if (ir->ir_reg_offset == -1 || ir->ir_vol_bit_shift == -1 ||
	    ir->ir_vol_bit_width == -1 || ir->ir_min_bit_val == -1 ||
	    ir->ir_min_voltage == -1 || ir->ir_max_voltage == -1)
		return;

	ir->ir_delay_reg_offset =
	    OF_getpropint(node, "anatop-delay-reg-offset", 0);
	ir->ir_delay_bit_shift =
	    OF_getpropint(node, "anatop-delay-bit-shift", 0);
	ir->ir_delay_bit_width =
	    OF_getpropint(node, "anatop-delay-bit-width", 0);

	ir->ir_rd.rd_node = node;
	ir->ir_rd.rd_cookie = ir;
	ir->ir_rd.rd_get_voltage = imxanatop_get_voltage;
	ir->ir_rd.rd_set_voltage = imxanatop_set_voltage;
	regulator_register(&ir->ir_rd);
}

uint32_t
imxanatop_get_voltage(void *cookie)
{
	struct imxanatop_regulator *ir = cookie;
	uint32_t bit_val;

	bit_val = HREAD4(ir->ir_sc, ir->ir_reg_offset) >> ir->ir_vol_bit_shift;
	bit_val &= ((1 << ir->ir_vol_bit_width) - 1);
	return (ir->ir_min_voltage + (bit_val - ir->ir_min_bit_val) * 25000);
}

int
imxanatop_set_voltage(void *cookie, uint32_t voltage)
{
	struct imxanatop_regulator *ir = cookie;
	uint32_t bit_val, old_bit_val, reg;
	int steps, usecs;

	if (voltage < ir->ir_min_voltage || voltage > ir->ir_max_voltage)
		return -1;

	bit_val = ir->ir_min_bit_val + (voltage - ir->ir_min_voltage) / 25000;
	reg = HREAD4(ir->ir_sc, ir->ir_reg_offset);
	old_bit_val = (reg >> ir->ir_vol_bit_shift);
	old_bit_val &= ((1 << ir->ir_vol_bit_width) -1);
	reg &= ~((1 << ir->ir_vol_bit_width) - 1) << ir->ir_vol_bit_shift;
	reg |= (bit_val << ir->ir_vol_bit_shift);
	HWRITE4(ir->ir_sc, ir->ir_reg_offset, reg);

	steps = bit_val - old_bit_val;
	if (steps > 0 && ir->ir_delay_bit_width > 0) {
		reg = HREAD4(ir->ir_sc, ir->ir_delay_reg_offset);
		reg >>= ir->ir_delay_bit_shift;
		reg &= ((1 << ir->ir_delay_bit_width) - 1);
		usecs = ((reg + 1) * steps * 64 * 1000000) / 24000000;
		delay(usecs);
	}

	return 0;
}
