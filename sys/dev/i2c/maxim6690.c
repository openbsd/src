/*	$OpenBSD: maxim6690.c,v 1.1 2005/11/15 16:24:49 deraadt Exp $	*/

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

/* Maxim 6690 registers */
#define MAXIM6690_INT_TEMP	0x00
#define MAXIM6690_EXT_TEMP	0x01
#define MAXIM6690_INT_TEMP2	0x10
#define MAXIM6690_EXT_TEMP2	0x11
#define MAXIM6690_STATUS	0x02
#define MAXIM6690_DEVID		0xfe
#define MAXIM6690_REVISION	0xff

#define MAXIM6690_TEMP_INVALID	0x80	/* sensor disconnected */

/* Sensors */
#define MAXTMP_INT		0
#define MAXTMP_EXT		1
#define MAXTMP_NUM_SENSORS	2

struct maxtmp_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct sensor sc_sensor[MAXTMP_NUM_SENSORS];
};

int	maxtmp_match(struct device *, void *, void *);
void	maxtmp_attach(struct device *, struct device *, void *);
int	maxtmp_check(struct i2c_attach_args *, u_int8_t *, u_int8_t *);
void	maxtmp_refresh(void *);

struct cfattach maxtmp_ca = {
	sizeof(struct maxtmp_softc), maxtmp_match, maxtmp_attach
};

struct cfdriver maxtmp_cd = {
	NULL, "maxtmp", DV_DULL
};

int
maxtmp_check(struct i2c_attach_args *ia, u_int8_t *idp, u_int8_t *revp)
{
	u_int8_t cmd, id, rev;

	iic_acquire_bus(ia->ia_tag, 0);

	cmd = MAXIM6690_DEVID;
	if (iic_exec(ia->ia_tag, I2C_OP_READ_WITH_STOP,
	    ia->ia_addr, &cmd, sizeof cmd, &id, sizeof id, 0)) {
		iic_release_bus(ia->ia_tag, 0);
		return 0;
	}

	cmd = MAXIM6690_REVISION;
	if (iic_exec(ia->ia_tag, I2C_OP_READ_WITH_STOP,
	    ia->ia_addr, &cmd, sizeof cmd, &rev, sizeof rev, 0)) {
		iic_release_bus(ia->ia_tag, 0);
		return 0;
	}

	iic_release_bus(ia->ia_tag, 0);

	if (id == 0x4d  && rev == 0x09) {
		*idp = id;
		*revp = rev;
		return (1);
	}
	return (0);
}


int
maxtmp_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	u_int8_t id, rev;

	if (ia->ia_compat) {
		if (strcmp(ia->ia_compat, "max6690") == 0)
			return (1);
		return (0);
	}
	return (maxtmp_check(ia, &id, &rev));
}

void
maxtmp_attach(struct device *parent, struct device *self, void *aux)
{
	struct maxtmp_softc *sc = (struct maxtmp_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t id, rev;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	rev = maxtmp_check(ia, &id, &rev);
	printf(": id 0x%x rev 0x%x", id, rev);

	/* Initialize sensor data. */
	for (i = 0; i < MAXTMP_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[MAXTMP_INT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[MAXTMP_INT].desc, "Internal",
	    sizeof(sc->sc_sensor[MAXTMP_INT].desc));

	sc->sc_sensor[MAXTMP_EXT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[MAXTMP_EXT].desc, "External",
	    sizeof(sc->sc_sensor[MAXTMP_EXT].desc));

	if (sensor_task_register(sc, maxtmp_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < MAXTMP_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void	maxtmp_readport(struct maxtmp_softc *, u_int8_t, u_int8_t, int);

void
maxtmp_readport(struct maxtmp_softc *sc, u_int8_t cmd1, u_int8_t cmd2,
    int index)
{
	u_int8_t data, data2;
	
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd1, sizeof cmd1, &data, sizeof data, 0))
		goto invalid;
	if (data == MAXIM6690_TEMP_INVALID)
		goto invalid;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd2, sizeof cmd2, &data2, sizeof data2, 0))
		goto invalid;

	sc->sc_sensor[index].value = 273150000 +
	    1000000 * data + (data2 >> 5) * 1000000 / 16;
	return;

invalid:
	sc->sc_sensor[index].flags |= SENSOR_FINVALID;
}

void
maxtmp_refresh(void *arg)
{
	struct maxtmp_softc *sc = arg;

	iic_acquire_bus(sc->sc_tag, 0);

	maxtmp_readport(sc, MAXIM6690_INT_TEMP, MAXIM6690_INT_TEMP2, MAXTMP_INT);
	maxtmp_readport(sc, MAXIM6690_EXT_TEMP, MAXIM6690_EXT_TEMP2, MAXTMP_EXT);

	iic_release_bus(sc->sc_tag, 0);
}
