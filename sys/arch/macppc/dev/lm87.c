/*	$OpenBSD: lm87.c,v 1.2 2005/11/14 22:22:32 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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

#include <dev/ofw/openfirm.h>
#include <arch/macppc/dev/maci2cvar.h>

/* LM87 registers */
#define LM87_2_5V	0x20
#define LM87_VCCP1	0x21
#define LM87_VCC	0x22
#define LM87_5V		0x23
#define LM87_12V	0x24
#define LM87_VCCP2	0x25
#define LM87_EXT_TEMP	0x26
#define LM87_INT_TEMP	0x27
#define LM87_FAN1	0x28
#define LM87_FAN2	0x28
#define LM87_REVISION	0x3f
#define LM87_FANDIV	0x47

/* Sensors */
#define LMENV_2_5V		0
#define LMENV_VCCP1		1
#define LMENV_VCC		2
#define LMENV_5V		3
#define LMENV_12V		4
#define LMENV_VCCP2		5
#define LMENV_EXT_TEMP		6
#define LMENV_INT_TEMP		7
#define LMENV_FAN1		8
#define LMENV_FAN2		9
#define LMENV_NUM_SENSORS	10

struct lmenv_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct sensor sc_sensor[LMENV_NUM_SENSORS];
	int sc_fan1_div, sc_fan2_div;
};

int	lmenv_match(struct device *, void *, void *);
void	lmenv_attach(struct device *, struct device *, void *);

void	lmenv_refresh(void *);

struct cfattach lmenv_ca = {
	sizeof(struct lmenv_softc), lmenv_match, lmenv_attach
};

struct cfdriver lmenv_cd = {
	NULL, "lmenv", DV_DULL
};

int
lmenv_match(struct device *parent, void *match, void *aux)
{
	struct maci2c_attach_args *ia = aux;
	char compat[32], name[32];

	memset(compat, 0, sizeof compat);
	OF_getprop(ia->ia_node, "compatible", &compat, sizeof compat);
	if (strcmp(compat, "lm87cimt") == 0)
		return (1);

	memset(name, 0, sizeof name);
	OF_getprop(ia->ia_node, "name", &name, sizeof name);
	if (strcmp(name, "lm87") == 0)
		return (1);

	return (0);
}

void
lmenv_attach(struct device *parent, struct device *self, void *aux)
{
	struct lmenv_softc *sc = (struct lmenv_softc *)self;
	struct maci2c_attach_args *ia = aux;
	u_int8_t cmd, data;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = LM87_FANDIV;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		     sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read Fan Divisor register\n");
		return;
	}
	sc->sc_fan1_div = (data >> 4) & 0x03;
	sc->sc_fan2_div = (data >> 6) & 0x03;

	cmd = LM87_REVISION;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		     sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read ID register\n");
		return;
	}

	iic_release_bus(sc->sc_tag, 0);

	printf(": LM87 rev %x", data);

	/* Initialize sensor data. */
	for (i = 0; i < LMENV_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[LMENV_2_5V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_2_5V].desc, "+2.5Vin",
	    sizeof(sc->sc_sensor[LMENV_2_5V].desc));

	sc->sc_sensor[LMENV_VCCP1].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCCP1].desc, "Vccp1",
	    sizeof(sc->sc_sensor[LMENV_VCCP1].desc));

	sc->sc_sensor[LMENV_VCC].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCC].desc, "+Vcc",
	    sizeof(sc->sc_sensor[LMENV_VCC].desc));

	sc->sc_sensor[LMENV_5V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_5V].desc, "+5Vin/Vcc",
	    sizeof(sc->sc_sensor[LMENV_5V].desc));

	sc->sc_sensor[LMENV_12V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_12V].desc, "+12Vin",
	    sizeof(sc->sc_sensor[LMENV_12V].desc));

	sc->sc_sensor[LMENV_VCCP2].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCCP2].desc, "Vccp2",
	    sizeof(sc->sc_sensor[LMENV_VCCP2].desc));

	sc->sc_sensor[LMENV_EXT_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[LMENV_EXT_TEMP].desc, "Ext. Temp.",
	    sizeof(sc->sc_sensor[LMENV_EXT_TEMP].desc));

	sc->sc_sensor[LMENV_INT_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[LMENV_INT_TEMP].desc, "Int. Temp.",
	    sizeof(sc->sc_sensor[LMENV_INT_TEMP].desc));

	sc->sc_sensor[LMENV_FAN1].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[LMENV_FAN1].desc, "FAN1",
	    sizeof(sc->sc_sensor[LMENV_FAN1].desc));

	sc->sc_sensor[LMENV_FAN2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[LMENV_FAN2].desc, "FAN2",
	    sizeof(sc->sc_sensor[LMENV_FAN2].desc));

	if (sensor_task_register(sc, lmenv_refresh, 5)) {
		printf(": unable to register update task\n");
		return;
	}

	for (i = 0; i < LMENV_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
lmenv_refresh(void *arg)
{
	struct lmenv_softc *sc = arg;
	u_int8_t cmd, data;
	int sensor;

	iic_acquire_bus(sc->sc_tag, 0);

	for (sensor = 0; sensor < LMENV_NUM_SENSORS; sensor++) {
		cmd = LM87_2_5V + sensor;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
			continue;
		}

		switch (sensor) {
		case LMENV_2_5V:
			sc->sc_sensor[sensor].value = 2500000 * data / 192;
			break;
		case LMENV_5V:
			sc->sc_sensor[sensor].value = 5000000 * data / 192;
			break;
		case LMENV_12V:
			sc->sc_sensor[sensor].value = 12000000 * data / 192;
			break;
		case LMENV_VCCP1:
		case LMENV_VCCP2:
			sc->sc_sensor[sensor].value = 2700000 * data / 192;
			break;
		case LMENV_VCC:
			sc->sc_sensor[sensor].value = 3300000 * data / 192;
			break;
		case LMENV_EXT_TEMP:
		case LMENV_INT_TEMP:
			sc->sc_sensor[sensor].value =
			    (int8_t)data * 1000000 + 273150000;
			break;
		case LMENV_FAN1:
			sc->sc_sensor[sensor].value =
			    (1350000 * data) / sc->sc_fan1_div;
			break;
		case LMENV_FAN2:
			sc->sc_sensor[sensor].value =
			    (1350000 * data) / sc->sc_fan2_div;
			break;
		default:
			sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
			continue;
		}
		sc->sc_sensor[sensor].flags &= ~SENSOR_FINVALID;
	}

	iic_release_bus(sc->sc_tag, 0);
}
