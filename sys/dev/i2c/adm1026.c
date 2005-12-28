/*	$OpenBSD: adm1026.c,v 1.1 2005/12/28 00:21:43 deraadt Exp $	*/

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

/* ADM 1026 registers */
#define ADM1026_TEMP		0x1f
#define ADM1026_STATUS		0x20
#define ADM1026_Vbat		0x26
#define ADM1026_Ain8		0x27
#define ADM1026_EXT1		0x28
#define ADM1026_EXT2		0x29
#define ADM1026_V3_3stby	0x2a
#define ADM1026_V3_3main	0x2b
#define ADM1026_V5		0x2c
#define ADM1026_Vccp		0x2d
#define ADM1026_V12		0x2e
#define ADM1026_Vminus12	0x2f
#define ADM1026_FAN0		0x38
#define ADM1026_FAN1		0x39
#define ADM1026_FAN2		0x3a
#define ADM1026_FAN3		0x3b
#define ADM1026_FAN4		0x3c
#define ADM1026_FAN5		0x3d
#define ADM1026_FAN6		0x3e
#define ADM1026_FAN7		0x3f
#define ADM1026_EXT1_OFF	0x6e
#define ADM1026_EXT2_OFF	0x6f
#define ADM1026_FAN0123DIV	0x02
#define ADM1026_FAN4567DIV	0x03
#define ADM1026_CONTROL		0x00
#define  ADM1026_CONTROL_START	0x01
#define  ADM1026_CONTROL_INTCLR	0x04

/* Sensors */
#define ADMCTS_TEMP		0
#define ADMCTS_EXT1		1
#define ADMCTS_EXT2		2
#define ADMCTS_Vbat		3
#define ADMCTS_V3_3stby		4
#define ADMCTS_V3_3main		5
#define ADMCTS_V5		6
#define ADMCTS_Vccp		7
#define ADMCTS_V12		8
#define ADMCTS_Vminus12		9
#define ADMCTS_FAN0		10
#define ADMCTS_FAN1		11
#define ADMCTS_FAN2		12
#define ADMCTS_FAN3		13
#define ADMCTS_FAN4		14
#define ADMCTS_FAN5		15
#define ADMCTS_FAN6		16
#define ADMCTS_FAN7		17
#define ADMCTS_NUM_SENSORS	18

struct admcts_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	struct sensor	sc_sensor[ADMCTS_NUM_SENSORS];
};

int	admcts_match(struct device *, void *, void *);
void	admcts_attach(struct device *, struct device *, void *);
void	admcts_refresh(void *);

struct cfattach admcts_ca = {
	sizeof(struct admcts_softc), admcts_match, admcts_attach
};

struct cfdriver admcts_cd = {
	NULL, "admcts", DV_DULL
};

int
admcts_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "adm1026") == 0)
		return (1);
	return (0);
}

void
admcts_attach(struct device *parent, struct device *self, void *aux)
{
	struct admcts_softc *sc = (struct admcts_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data, data2;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);
	cmd = ADM1026_CONTROL;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot get control register\n");
		return;
	}
	data2 = data | ADM1026_CONTROL_START;
	data2 = data2 & ~ADM1026_CONTROL_INTCLR;
	if (data != data2) {
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data2, sizeof data2, I2C_F_POLL)) {
			iic_release_bus(sc->sc_tag, 0);
			printf(": cannot set control register\n");
			return;
		}
	}
	iic_release_bus(sc->sc_tag, 0);

	/* Initialize sensor data. */
	for (i = 0; i < ADMCTS_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[ADMCTS_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMCTS_TEMP].desc, "Internal",
	    sizeof(sc->sc_sensor[ADMCTS_TEMP].desc));

	sc->sc_sensor[ADMCTS_Vbat].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_Vbat].desc, "Vbat",
	    sizeof(sc->sc_sensor[ADMCTS_Vbat].desc));

	sc->sc_sensor[ADMCTS_EXT1].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMCTS_EXT1].desc, "External1",
	    sizeof(sc->sc_sensor[ADMCTS_EXT1].desc));

	sc->sc_sensor[ADMCTS_EXT2].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADMCTS_EXT2].desc, "External2",
	    sizeof(sc->sc_sensor[ADMCTS_EXT2].desc));

	sc->sc_sensor[ADMCTS_V3_3stby].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_V3_3stby].desc, "3.3 V standby",
	    sizeof(sc->sc_sensor[ADMCTS_V3_3stby].desc));

	sc->sc_sensor[ADMCTS_V3_3main].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_V3_3main].desc, "3.3 V main",
	    sizeof(sc->sc_sensor[ADMCTS_V3_3main].desc));

	sc->sc_sensor[ADMCTS_V5].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_V5].desc, "5 V",
	    sizeof(sc->sc_sensor[ADMCTS_V5].desc));

	sc->sc_sensor[ADMCTS_Vccp].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_Vccp].desc, "Vccp",
	    sizeof(sc->sc_sensor[ADMCTS_Vccp].desc));

	sc->sc_sensor[ADMCTS_V12].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_V12].desc, "12 V",
	    sizeof(sc->sc_sensor[ADMCTS_V12].desc));

	sc->sc_sensor[ADMCTS_Vminus12].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADMCTS_Vminus12].desc, "-12 V",
	    sizeof(sc->sc_sensor[ADMCTS_Vminus12].desc));

	sc->sc_sensor[ADMCTS_FAN1].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN1].desc, "Fan1",
	    sizeof(sc->sc_sensor[ADMCTS_FAN1].desc));

	sc->sc_sensor[ADMCTS_FAN2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN2].desc, "Fan2",
	    sizeof(sc->sc_sensor[ADMCTS_FAN2].desc));

	sc->sc_sensor[ADMCTS_FAN2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN2].desc, "Fan2",
	    sizeof(sc->sc_sensor[ADMCTS_FAN2].desc));

	sc->sc_sensor[ADMCTS_FAN3].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN3].desc, "Fan3",
	    sizeof(sc->sc_sensor[ADMCTS_FAN3].desc));

	sc->sc_sensor[ADMCTS_FAN4].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN4].desc, "Fan4",
	    sizeof(sc->sc_sensor[ADMCTS_FAN4].desc));

	sc->sc_sensor[ADMCTS_FAN5].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN5].desc, "Fan5",
	    sizeof(sc->sc_sensor[ADMCTS_FAN5].desc));

	sc->sc_sensor[ADMCTS_FAN6].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN6].desc, "Fan6",
	    sizeof(sc->sc_sensor[ADMCTS_FAN6].desc));

	sc->sc_sensor[ADMCTS_FAN7].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADMCTS_FAN7].desc, "Fan7",
	    sizeof(sc->sc_sensor[ADMCTS_FAN7].desc));

	if (sensor_task_register(sc, admcts_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < ADMCTS_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
admcts_refresh(void *arg)
{
	struct admcts_softc *sc = arg;
	u_int8_t cmd, data;
	int8_t sdata;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = ADM1026_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata, sizeof sdata, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_TEMP].value = 273150000 + 1000000 * sdata;

	cmd = ADM1026_EXT1;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata, sizeof sdata, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_EXT1].value = 273150000 + 1000000 * sdata;

	cmd = ADM1026_EXT2;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &sdata, sizeof sdata, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_EXT2].value = 273150000 + 1000000 * sdata;

	cmd = ADM1026_Vbat;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_Vbat].value = 3000000 * data / 192;

	cmd = ADM1026_V3_3stby;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_V3_3stby].value = 3300000 * data / 192;

	cmd = ADM1026_V3_3main;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_V3_3main].value = 3300000 * data / 192;

	cmd = ADM1026_V5;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_V5].value = 5500000 * data / 192;

	cmd = ADM1026_Vccp;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_Vccp].value = 2250000 * data / 192;

	cmd = ADM1026_V12;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_V12].value = 12000000 * data / 192;

	cmd = ADM1026_Vminus12;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, I2C_F_POLL) == 0)
		sc->sc_sensor[ADMCTS_Vminus12].value = -2125000 * data / 192;

	/* XXX read fan values */

	iic_release_bus(sc->sc_tag, 0);
}
