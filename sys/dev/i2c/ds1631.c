/*	$OpenBSD: ds1631.c,v 1.2 2005/12/27 17:18:18 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Theo de Raadt
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

/* Maxim ds 1631 registers */
#define DS1631_TEMP		0xaa

/* Sensors */
#define MAXDS_TEMP		0
#define MAXDS_NUM_SENSORS	1

struct maxds_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	struct sensor	sc_sensor[MAXDS_NUM_SENSORS];
};

int	maxds_match(struct device *, void *, void *);
void	maxds_attach(struct device *, struct device *, void *);
void	maxds_refresh(void *);

struct cfattach maxds_ca = {
	sizeof(struct maxds_softc), maxds_match, maxds_attach
};

struct cfdriver maxds_cd = {
	NULL, "maxds", DV_DULL
};

int
maxds_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "ds1631") == 0)
		return (1);
	return (0);
}

void
maxds_attach(struct device *parent, struct device *self, void *aux)
{
	struct maxds_softc *sc = (struct maxds_softc *)self;
	struct i2c_attach_args *ia = aux;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/* Initialize sensor data. */
	for (i = 0; i < MAXDS_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[MAXDS_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[MAXDS_TEMP].desc, "Internal",
	    sizeof(sc->sc_sensor[MAXDS_TEMP].desc));

	if (sensor_task_register(sc, maxds_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < MAXDS_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
maxds_refresh(void *arg)
{
	struct maxds_softc *sc = arg;
	u_int8_t cmd, data[2];

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = DS1631_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[MAXDS_TEMP].value = 273150000 +
		    1000000 * data[1] + 1000000 * data[0] / 256;

	iic_release_bus(sc->sc_tag, 0);
}
