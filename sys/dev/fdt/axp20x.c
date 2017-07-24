/* $OpenBSD: axp20x.c,v 1.2 2017/07/24 20:35:26 kettenis Exp $ */
/*
 * Copyright (c) 2014,2016 Artturi Alm
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
#include <sys/sensors.h>

#include <dev/i2c/i2cvar.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <armv7/armv7/armv7_machdep.h>	/* needed for powerdownfn */

/* Power Status Register / Input power status */
#define	AXP209_PSR		0x00
#define	AXP209_PSR_ACIN		(1 << 7)	/* ACIN Exists */
#define	AXP209_PSR_VBUS		(1 << 5)	/* VBUS Exists */

/* Shutdown settings, battery detection, and CHGLED Pin control */
#define	AXP209_SDR		0x32
#define	AXP209_SDR_SHUTDOWN	(1 << 7)	/* Shutdown Control */

struct axp20x_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;
};

int	axp20x_match(struct device *, void *, void *);
void	axp20x_attach(struct device *, struct device *, void *);

int	axp20x_readb(uint8_t, uint8_t *);
int	axp20x_writeb(uint8_t, uint8_t);
void	axp20x_shutdown(void);

struct cfattach axppmic_ca = {
	sizeof(struct axp20x_softc), axp20x_match, axp20x_attach
};

struct cfdriver axppmic_cd = {
	NULL, "axppmic", DV_DULL
};

int
axp20x_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;

	return (OF_is_compatible(node, "x-powers,axp152") ||
	    OF_is_compatible(node, "x-powers,axp209"));
}

void
axp20x_attach(struct device *parent, struct device *self, void *aux)
{
	struct axp20x_softc *sc = (struct axp20x_softc *)self;
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;
	uint8_t psr;

	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (OF_is_compatible(node, "x-powers,axp152")) {
		printf(": AXP152");
	} else {
		axp20x_readb(AXP209_PSR, &psr);
		printf(": AXP209,");
		if ((psr & (AXP209_PSR_ACIN | AXP209_PSR_VBUS)) == 0)
			printf(" BAT");
		else {
			if (psr & AXP209_PSR_ACIN)
				printf(" ACIN");
			if (psr & AXP209_PSR_VBUS)
				printf(" VBUS");
		}
	}
	printf("\n");

	powerdownfn = axp20x_shutdown;
}

int
axp20x_readb(uint8_t reg, uint8_t *val)
{
	struct axp20x_softc *sc = axppmic_cd.cd_devs[0];
	int flags = I2C_F_POLL;
	int ret;

	if (sc == NULL)
		return ENXIO;

	iic_acquire_bus(sc->sc_i2c, flags);
	ret = iic_smbus_read_byte(sc->sc_i2c, sc->sc_addr, reg, val, flags);
	iic_release_bus(sc->sc_i2c, flags);
	return ret;

}

int
axp20x_writeb(uint8_t reg, uint8_t data)
{
	struct axp20x_softc *sc = axppmic_cd.cd_devs[0];
	int flags = I2C_F_POLL;
	int ret;

	if (sc == NULL)
		return ENXIO;

	iic_acquire_bus(sc->sc_i2c, flags);
	ret = iic_smbus_write_byte(sc->sc_i2c, sc->sc_addr, reg, data, flags);
	iic_release_bus(sc->sc_i2c, flags);

	return ret;
}

void
axp20x_shutdown(void)
{
	axp20x_writeb(AXP209_SDR, AXP209_SDR_SHUTDOWN);
}
