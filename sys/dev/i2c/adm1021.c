/*	$OpenBSD: adm1021.c,v 1.5 2005/12/24 23:09:19 deraadt Exp $	*/

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

/* ADM 1021 registers */
#define ADM1021_INT_TEMP	0x00
#define ADM1021_EXT_TEMP	0x01
#define ADM1021_STATUS		0x02
#define ADM1021_CONFIG_READ	0x03
#define ADM1021_CONFIG_WRITE	0x09

#define ADM1021_COMPANY		0xfe	/* contains 0x41 */
#define ADM1021_STEPPING	0xff	/* contains 0x3? */

/* Sensors */
#define ADMTEMP_INT		0
#define ADMTEMP_EXT		1
#define ADMTEMP_NUM_SENSORS	2

struct admtemp_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	struct sensor	sc_sensor[ADMTEMP_NUM_SENSORS];
	int		sc_xeon;
};

int	admtemp_match(struct device *, void *, void *);
void	admtemp_attach(struct device *, struct device *, void *);
void	admtemp_refresh(void *);

struct cfattach admtemp_ca = {
	sizeof(struct admtemp_softc), admtemp_match, admtemp_attach
};

struct cfdriver admtemp_cd = {
	NULL, "admtemp", DV_DULL
};

int
admtemp_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "adm1021") == 0 ||
	    strcmp(ia->ia_name, "xeon") == 0) {
		/*
		 * should also ensure that
		 * config & 0x80 == 0x00
		 * status1 & 0xc0 == 0x00
		 * status2 & 0xbc == 0x00
		 * before accepting this to be for real
		 */
		return (1);
	}
	return (0);
}

void
admtemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct admtemp_softc *sc = (struct admtemp_softc *)self;
	struct i2c_attach_args *ia = aux;
//	u_int8_t cmd, data;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (ia->ia_name && strcmp(ia->ia_name, "xeon") == 0) {
		printf(": Xeon\n");
		sc->sc_xeon = 1;
	}

//	iic_acquire_bus(sc->sc_tag, 0);
//	cmd = ADM1021_CONFIG_READ;
//	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
//	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
//		iic_release_bus(sc->sc_tag, 0);
//		printf(": cannot get control register\n");
//		return;
//	}
//	data &= ~0x40;
//	cmd = ADM1021_CONFIG_WRITE;
//	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
//	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
//		iic_release_bus(sc->sc_tag, 0);
//		printf(": cannot set control register\n");
//		return;
//	}
//	iic_release_bus(sc->sc_tag, 0);

	/* Initialize sensor data. */
	for (i = 0; i < ADMTEMP_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[ADMTEMP_INT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMTEMP_INT].desc, "Internal",
	    sizeof(sc->sc_sensor[ADMTEMP_INT].desc));
	if (sc->sc_xeon)
		sc->sc_sensor[ADMTEMP_INT].flags |= SENSOR_FINVALID;	

	sc->sc_sensor[ADMTEMP_EXT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMTEMP_EXT].desc, "External",
	    sizeof(sc->sc_sensor[ADMTEMP_EXT].desc));

	if (sensor_task_register(sc, admtemp_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < ADMTEMP_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
admtemp_refresh(void *arg)
{
	struct admtemp_softc *sc = arg;
	u_int8_t cmd;
	int8_t sdata;

	iic_acquire_bus(sc->sc_tag, 0);

	if (sc->sc_xeon == 0) {
		cmd = ADM1021_INT_TEMP;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &sdata,
		    sizeof sdata, I2C_F_POLL) == 0)
			sc->sc_sensor[ADMTEMP_INT].value =
			    273150000 + 1000000 * sdata;
	}

	cmd = ADM1021_EXT_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata,
	    sizeof sdata, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMTEMP_EXT].value = 273150000 + 1000000 * sdata;

	iic_release_bus(sc->sc_tag, 0);
}
