/*	$OpenBSD: fanpwr.c,v 1.2 2018/07/31 10:07:13 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/i2c/i2cvar.h>

/* Registers */
#define FAN53555_VSEL0			0x00
#define FAN53555_VSEL1			0x01
#define  FAN53555_VSEL_NSEL_MASK	0x3f
#define FAN53555_CONTROL		0x02
#define  FAN53555_CONTROL_SLEW_MASK	(0x7 << 4)
#define  FAN53555_CONTROL_SLEW_SHIFT	4
#define FAN53555_ID1			0x03
#define FAN53555_ID2			0x04

/* Distinguish between Failrchild original and Silergy clones. */
enum fanpwr_id {
	FANPWR_FAN53555,	/* Fairchild FAN53555 */
	FANPWR_SYR827,		/* Silergy SYR827 */
	FANPWR_SYR828		/* Silergy SYR828 */
};

struct fanpwr_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	enum fanpwr_id	sc_id;
	uint8_t		sc_vsel;

	struct regulator_device sc_rd;
	uint32_t	sc_vbase;
	uint32_t	sc_vstep;
};

int	fanpwr_match(struct device *, void *, void *);
void	fanpwr_attach(struct device *, struct device *, void *);

struct cfattach fanpwr_ca = {
	sizeof(struct fanpwr_softc), fanpwr_match, fanpwr_attach
};

struct cfdriver fanpwr_cd = {
	NULL, "fanpwr", DV_DULL
};

uint8_t	fanpwr_read(struct fanpwr_softc *, int);
void	fanpwr_write(struct fanpwr_softc *, int, uint8_t);
uint32_t fanpwr_get_voltage(void *);
int	fanpwr_set_voltage(void *, uint32_t);

int
fanpwr_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;

	return (OF_is_compatible(node, "fcs,fan53555") ||
	    OF_is_compatible(node, "silergy,syr827") ||
	    OF_is_compatible(node, "silergy,syr828"));
}

void
fanpwr_attach(struct device *parent, struct device *self, void *aux)
{
	struct fanpwr_softc *sc = (struct fanpwr_softc *)self;
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;
	uint32_t voltage, ramp_delay;
	uint8_t id1, id2;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (OF_getpropint(node, "fcs,suspend-voltage-selector", 0))
		sc->sc_vsel = FAN53555_VSEL0;
	else
		sc->sc_vsel = FAN53555_VSEL1;

	if (OF_is_compatible(node, "silergy,syr827")) {
		printf(": SYR827");
		sc->sc_id = FANPWR_SYR827;
	} else if (OF_is_compatible(node, "silergy,syr828")) {
		printf(": SYR828");
		sc->sc_id = FANPWR_SYR828;
	} else {
		printf(": FAN53555");
		sc->sc_id = FANPWR_FAN53555;
	}

	id1 = fanpwr_read(sc, FAN53555_ID1);
	id2 = fanpwr_read(sc, FAN53555_ID2);

	switch (sc->sc_id) {
	case FANPWR_FAN53555:
		switch (id1 << 8 | id2) {
		case 0x8003:	/* 00 Option */
		case 0x8103:	/* 01 Option */
		case 0x8303:	/* 03 Option */
		case 0x8503:	/* 05 Option */
		case 0x8801:	/* 08, 18 Options */
		case 0x880f:	/* BUC08, BUC18 Options */
		case 0x8108:	/* 79 Option */
			sc->sc_vbase = 600000;
			sc->sc_vstep = 10000;
			break;
		case 0x840f:	/* 04 Option */
		case 0x8c0f:	/* 09 Option */
			sc->sc_vbase = 603000;
			sc->sc_vstep = 12826;
			break;
		case 0x800f:	/* 13 Option */
			sc->sc_vbase = 800000;
			sc->sc_vstep = 10000;
			break;
		case 0x800c:	/* 23 Option */
			sc->sc_vbase = 600000;
			sc->sc_vstep = 12500;
			break;
		case 0x8004:	/* 24 Option */
			sc->sc_vbase = 603000;
			sc->sc_vstep = 12967;
			break;
		default:
			printf(", unknown ID1 0x%02x ID2 0x%02x\n", id1, id2);
			return;
		}
		break;
	case FANPWR_SYR827:
	case FANPWR_SYR828:
		sc->sc_vbase = 712500;
		sc->sc_vstep = 12500;
		break;
	}

	voltage = fanpwr_get_voltage(sc);
	printf(", %d.%02d VDC", voltage / 1000000,
		    (voltage % 1000000) / 10000);

	ramp_delay = OF_getpropint(node, "regulator-ramp-delay", 0);
	if (ramp_delay > 0) {
		uint8_t ctrl, slew;

		for (slew = 7; slew > 0; slew--)
			if ((64000 >> slew) >= ramp_delay)
				break;
		ctrl = fanpwr_read(sc, FAN53555_CONTROL);
		ctrl &= ~FAN53555_CONTROL_SLEW_MASK;
		ctrl |= slew << FAN53555_CONTROL_SLEW_SHIFT;
		fanpwr_write(sc, FAN53555_CONTROL, ctrl);
	}

	sc->sc_rd.rd_node = node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_get_voltage = fanpwr_get_voltage;
	sc->sc_rd.rd_set_voltage = fanpwr_set_voltage;
	regulator_register(&sc->sc_rd);

	printf("\n");
}

uint8_t
fanpwr_read(struct fanpwr_softc *sc, int reg)
{
	uint8_t cmd = reg;
	uint8_t val;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof cmd, &val, sizeof val, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("error %d\n", error);
		printf("%s: can't read register 0x%02x\n",
		    sc->sc_dev.dv_xname, reg);
		val = 0xff;
	}

	return val;
}

void
fanpwr_write(struct fanpwr_softc *sc, int reg, uint8_t val)
{
	uint8_t cmd = reg;
	int error;

	iic_acquire_bus(sc->sc_tag, I2C_F_POLL);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof cmd, &val, sizeof val, I2C_F_POLL);
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	if (error) {
		printf("%s: can't write register 0x%02x\n",
		    sc->sc_dev.dv_xname, reg);
	}
}

uint32_t
fanpwr_get_voltage(void *cookie)
{
	struct fanpwr_softc *sc = cookie;
	uint8_t vsel;
	
	vsel = fanpwr_read(sc, sc->sc_vsel);
	return sc->sc_vbase + (vsel & FAN53555_VSEL_NSEL_MASK) * sc->sc_vstep;
}

int
fanpwr_set_voltage(void *cookie, uint32_t voltage)
{
	struct fanpwr_softc *sc = cookie;
	uint32_t vmin = sc->sc_vbase;
	uint32_t vmax = vmin + FAN53555_VSEL_NSEL_MASK * sc->sc_vstep;
	uint8_t vsel;

	if (voltage < vmin || voltage > vmax)
		return EINVAL;

	vsel = fanpwr_read(sc, sc->sc_vsel);
	vsel &= ~FAN53555_VSEL_NSEL_MASK;
	vsel |= (voltage - sc->sc_vbase) / sc->sc_vstep;
	fanpwr_write(sc, sc->sc_vsel, vsel);

	return 0;
}
