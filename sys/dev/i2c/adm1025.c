/*	$OpenBSD: adm1025.c,v 1.10 2005/12/23 22:56:44 deraadt Exp $	*/

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

/* ADM 1025 registers */
#define ADM1025_V25		0x20
#define ADM1025_Vccp		0x21
#define ADM1025_V33		0x22
#define ADM1025_V5		0x23
#define ADM1025_V12		0x24
#define ADM1025_Vcc		0x25
#define ADM1025_EXT_TEMP	0x26
#define ADM1025_INT_TEMP	0x27
#define ADM1025_STATUS2		0x42
#define  ADM1025_STATUS2_EXT	0x40
#define ADM1025_COMPANY		0x3e	/* contains 0x41 */
#define ADM1025_STEPPING	0x3f	/* contains 0x2? */
#define ADM1025_CONFIG		0x40

/* Sensors */
#define ADMTM_INT		0
#define ADMTM_EXT		1
#define ADMTM_V25		2
#define ADMTM_Vccp		3
#define ADMTM_V33		4
#define ADMTM_V5		5
#define ADMTM_V12		6
#define ADMTM_Vcc		7
#define ADMTM_NUM_SENSORS	8

struct admtm_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	struct sensor	sc_sensor[ADMTM_NUM_SENSORS];
};

int	admtm_match(struct device *, void *, void *);
void	admtm_attach(struct device *, struct device *, void *);
void	admtm_refresh(void *);

struct cfattach admtm_ca = {
	sizeof(struct admtm_softc), admtm_match, admtm_attach
};

struct cfdriver admtm_cd = {
	NULL, "admtm", DV_DULL
};

int
admtm_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "adm1025") == 0 || 
	    strcmp(ia->ia_name, "47m192") == 0 ||
	    strcmp(ia->ia_name, "ne1619") == 0) {
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
admtm_attach(struct device *parent, struct device *self, void *aux)
{
	struct admtm_softc *sc = (struct admtm_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = ADM1025_CONFIG;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot get control register\n");
		return;
	}
	data &= ~0x01;
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot set control register\n");
		return;
	}
	iic_release_bus(sc->sc_tag, 0);

	/* Initialize sensor data. */
	for (i = 0; i < ADMTM_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[ADMTM_INT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMTM_INT].desc, "Internal",
	    sizeof(sc->sc_sensor[ADMTM_INT].desc));

	sc->sc_sensor[ADMTM_EXT].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMTM_EXT].desc, "External",
	    sizeof(sc->sc_sensor[ADMTM_EXT].desc));

	sc->sc_sensor[ADMTM_V25].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_V25].desc, "2.5 V",
	    sizeof(sc->sc_sensor[ADMTM_V25].desc));

	sc->sc_sensor[ADMTM_Vccp].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_Vccp].desc, "Vccp",
	    sizeof(sc->sc_sensor[ADMTM_Vccp].desc));

	sc->sc_sensor[ADMTM_V33].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_V33].desc, "3.3 V",
	    sizeof(sc->sc_sensor[ADMTM_V33].desc));

	sc->sc_sensor[ADMTM_V5].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_V5].desc, "5 V",
	    sizeof(sc->sc_sensor[ADMTM_V5].desc));

	sc->sc_sensor[ADMTM_V12].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_V12].desc, "12 V",
	    sizeof(sc->sc_sensor[ADMTM_V12].desc));

	sc->sc_sensor[ADMTM_Vcc].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMTM_Vcc].desc, "Vcc",
	    sizeof(sc->sc_sensor[ADMTM_Vcc].desc));

	if (sensor_task_register(sc, admtm_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < ADMTM_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
admtm_refresh(void *arg)
{
	struct admtm_softc *sc = arg;
	u_int8_t cmd, data, sdata;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = ADM1025_INT_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata, sizeof sdata, 0) == 0)
		sc->sc_sensor[ADMTM_INT].value = 273150000 + 1000000 * sdata;

	cmd = ADM1025_EXT_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata, sizeof sdata, 0) == 0)
		sc->sc_sensor[ADMTM_EXT].value = 273150000 + 1000000 * sdata;

	cmd = ADM1025_STATUS2;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0) {
		if (data & ADM1025_STATUS2_EXT)
			sc->sc_sensor[ADMTM_EXT].flags |= SENSOR_FINVALID;
		else
			sc->sc_sensor[ADMTM_EXT].flags &= ~SENSOR_FINVALID;
	}

	cmd = ADM1025_V25;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_V25].value = 2500000 * data / 192;

	cmd = ADM1025_Vccp;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_Vcc].value = 2249000 * data / 192;

	cmd = ADM1025_V33;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_V33].value = 3300000 * data / 192;

	cmd = ADM1025_V5;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_V5].value = 5000000 * data / 192;

	cmd = ADM1025_V12;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_V12].value = 12000000 * data / 192;

	cmd = ADM1025_Vcc;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0) == 0)
		sc->sc_sensor[ADMTM_Vcc].value = 3300000 * data / 192;

	iic_release_bus(sc->sc_tag, 0);
}
