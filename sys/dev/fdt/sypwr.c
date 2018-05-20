/*	$OpenBSD: sypwr.c,v 1.1 2017/12/31 11:13:59 kettenis Exp $	*/
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
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/i2c/i2cvar.h>

#define SY8106A_VOUT1_SEL		0x01
#define  SY8106A_VOUT1_SEL_I2C		(1 << 7)
#define  SY8106A_VOUT1_SEL_MASK 	0x7f

struct sypwr_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	struct regulator_device sc_rd;
};

int	sypwr_match(struct device *, void *, void *);
void	sypwr_attach(struct device *, struct device *, void *);

struct cfattach sypwr_ca = {
	sizeof(struct sypwr_softc), sypwr_match, sypwr_attach
};

struct cfdriver sypwr_cd = {
	NULL, "sypwr", DV_DULL
};

uint8_t	sypwr_read(struct sypwr_softc *, int);
void	sypwr_write(struct sypwr_softc *, int, uint8_t);
uint32_t sypwr_get_voltage(void *);
int	sypwr_set_voltage(void *, uint32_t voltage);

int
sypwr_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;

	return (OF_is_compatible(node, "silergy,sy8106a"));
}

void
sypwr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sypwr_softc *sc = (struct sypwr_softc *)self;
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;
	uint8_t reg;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/*
	 * Only register the regulator if it is under I2C control
	 * (i.e. initialized by the firmware).  Otherwise we have no
	 * idea what the current output voltage is, which will confuse
	 * the regulator framework.
	 */
	reg = sypwr_read(sc, SY8106A_VOUT1_SEL);
	if (reg & SY8106A_VOUT1_SEL_I2C) {
		uint32_t voltage;

		voltage = sypwr_get_voltage(sc);
		printf(": %d.%02d VDC", voltage / 1000000,
		    (voltage % 1000000) / 10000);

		sc->sc_rd.rd_node = node;
		sc->sc_rd.rd_cookie = sc;
		sc->sc_rd.rd_get_voltage = sypwr_get_voltage;
		sc->sc_rd.rd_set_voltage = sypwr_set_voltage;
		regulator_register(&sc->sc_rd);
	}

	printf("\n");
}

uint8_t
sypwr_read(struct sypwr_softc *sc, int reg)
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
sypwr_write(struct sypwr_softc *sc, int reg, uint8_t val)
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
sypwr_get_voltage(void *cookie)
{
	struct sypwr_softc *sc = cookie;
	uint8_t value;
	
	value = sypwr_read(sc, SY8106A_VOUT1_SEL);
	return 680000 + (value & SY8106A_VOUT1_SEL_MASK) * 10000;
}

int
sypwr_set_voltage(void *cookie, uint32_t voltage)
{
	struct sypwr_softc *sc = cookie;
	uint8_t value;

	if (voltage < 680000 || voltage > 1950000)
		return EINVAL;

	value = (voltage - 680000) / 10000;
	sypwr_write(sc, SY8106A_VOUT1_SEL, value | SY8106A_VOUT1_SEL_I2C);
	return 0;
}
