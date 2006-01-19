/*	$OpenBSD: adt7460.c,v 1.6 2006/01/19 17:08:39 grange Exp $	*/

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

#include <dev/i2c/i2cvar.h>

/* ADT7460 registers */
#define ADT7460_2_5V		0x20
#define ADT7460_VCCP1		0x22
#define ADT7460_REM1_TEMP	0x25
#define ADT7460_LOCAL_TEMP	0x26
#define ADT7460_REM2_TEMP	0x27
#define ADT7460_TACH1L		0x28
#define ADT7460_TACH1H		0x29
#define ADT7460_TACH2L		0x2a
#define ADT7460_TACH2H		0x2b
#define ADT7460_TACH3L		0x2c
#define ADT7460_TACH3H		0x2d
#define ADT7460_TACH4L		0x2e
#define ADT7460_TACH4H		0x2f
#define ADT7460_REVISION	0x3f
#define ADT7460_CONFIG		0x40

/* Sensors */
#define ADT_2_5V		0
#define ADT_VCCP1		1
#define ADT_REM1_TEMP		2
#define ADT_LOCAL_TEMP		3
#define ADT_REM2_TEMP		4
#define ADT_TACH1		5
#define ADT_TACH2		6
#define ADT_TACH3		7
#define ADT_TACH4		8
#define ADT_NUM_SENSORS		9

struct adt_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;
	int	sc_chip;

	struct sensor sc_sensor[ADT_NUM_SENSORS];
};

int	adt_match(struct device *, void *, void *);
void	adt_attach(struct device *, struct device *, void *);

void	adt_refresh(void *);

struct cfattach adt_ca = {
	sizeof(struct adt_softc), adt_match, adt_attach
};

struct cfdriver adt_cd = {
	NULL, "adt", DV_DULL
};

int
adt_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "adt7460") == 0 ||
	    strcmp(ia->ia_name, "adt7467") == 0 ||
	    strcmp(ia->ia_name, "adt7476") == 0 ||
	    strcmp(ia->ia_name, "adm1027") == 0 ||
	    strcmp(ia->ia_name, "lm85") == 0 ||
	    strcmp(ia->ia_name, "emc6d10x") == 0)
		return (1);
	return (0);
}

void
adt_attach(struct device *parent, struct device *self, void *aux)
{
	struct adt_softc *sc = (struct adt_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, rev, data;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	sc->sc_chip = 7460;
	/* check for the fancy "extension" chips XXX */
	if (strcmp(ia->ia_name, "adt7467") == 0 ||
	    strcmp(ia->ia_name, "adt7467") == 0)
		sc->sc_chip = 7467;

	cmd = ADT7460_REVISION;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &rev, sizeof rev, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read REV register\n");
		return;
	}

	if (sc->sc_chip == 7460) {
		data = 1;
		cmd = ADT7460_CONFIG;
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			iic_release_bus(sc->sc_tag, 0);
			printf(": cannot set control register\n");
			return;
		}
	}

	iic_release_bus(sc->sc_tag, 0);

	printf(": %s (ADT%d) rev %x", ia->ia_name, sc->sc_chip, rev);

	/* Initialize sensor data. */
	for (i = 0; i < ADT_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[ADT_2_5V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADT_2_5V].desc, "+2.5Vin",
	    sizeof(sc->sc_sensor[ADT_2_5V].desc));

	sc->sc_sensor[ADT_VCCP1].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[ADT_VCCP1].desc, "Vccp1",
	    sizeof(sc->sc_sensor[ADT_VCCP1].desc));

	sc->sc_sensor[ADT_REM1_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADT_REM1_TEMP].desc, "Rem1 Temp.",
	    sizeof(sc->sc_sensor[ADT_REM1_TEMP].desc));

	sc->sc_sensor[ADT_LOCAL_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADT_LOCAL_TEMP].desc, "Int. Temp.",
	    sizeof(sc->sc_sensor[ADT_LOCAL_TEMP].desc));

	sc->sc_sensor[ADT_REM2_TEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[ADT_REM2_TEMP].desc, "Rem1 Temp.",
	    sizeof(sc->sc_sensor[ADT_REM2_TEMP].desc));

	sc->sc_sensor[ADT_TACH1].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADT_TACH1].desc, "TACH1",
	    sizeof(sc->sc_sensor[ADT_TACH1].desc));

	sc->sc_sensor[ADT_TACH2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADT_TACH2].desc, "TACH1",
	    sizeof(sc->sc_sensor[ADT_TACH2].desc));

	sc->sc_sensor[ADT_TACH3].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADT_TACH3].desc, "TACH1",
	    sizeof(sc->sc_sensor[ADT_TACH3].desc));

	sc->sc_sensor[ADT_TACH4].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[ADT_TACH4].desc, "TACH1",
	    sizeof(sc->sc_sensor[ADT_TACH4].desc));

	if (sensor_task_register(sc, adt_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < ADT_NUM_SENSORS; i++)
		sensor_add(&sc->sc_sensor[i]);

	printf("\n");
}

struct {
	char		sensor;
	u_int8_t	cmd;
} worklist[] = {
	{ ADT_2_5V, ADT7460_2_5V },
	{ ADT_VCCP1, ADT7460_VCCP1 },
	{ ADT_REM1_TEMP, ADT7460_REM1_TEMP },
	{ ADT_LOCAL_TEMP, ADT7460_LOCAL_TEMP },
	{ ADT_REM2_TEMP, ADT7460_REM2_TEMP },
	{ ADT_TACH1, ADT7460_TACH1L },
	{ ADT_TACH2, ADT7460_TACH2L },
	{ ADT_TACH3, ADT7460_TACH3L },
	{ ADT_TACH4, ADT7460_TACH4L },
};

void
adt_refresh(void *arg)
{
	struct adt_softc *sc = arg;
	u_int8_t cmd, data, data2;
	u_int16_t fan;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	for (i = 0; i < sizeof worklist / sizeof(worklist[0]); i++) {
		cmd = worklist[i].cmd;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			continue;
		}

		sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		switch (worklist[i].sensor) {
		case ADT_2_5V:
			sc->sc_sensor[i].value = 2500000 * data / 192;
			break;
		case ADT_VCCP1:
			sc->sc_sensor[i].value = 2700000 * data / 192;
			break;
		case ADT_LOCAL_TEMP:
		case ADT_REM1_TEMP:
		case ADT_REM2_TEMP:
			if (data == 0x80)
				sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			else
				sc->sc_sensor[i].value =
				    (int8_t)data * 1000000 + 273150000;
			break;
		case ADT_TACH1:
		case ADT_TACH2:
		case ADT_TACH3:
		case ADT_TACH4:
			cmd = worklist[i].cmd + 1; /* TACHnH follows TACHnL */
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data2, sizeof data2, 0)) {
				sc->sc_sensor[i].flags |= SENSOR_FINVALID;
				continue;
			}

			fan = data + (data2 << 8);
			if (fan == 0 || fan == 0xffff)
				sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			else
				sc->sc_sensor[i].value = (90000 * 60) / fan;
			break;
		default:
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			break;
		}
	}

	iic_release_bus(sc->sc_tag, 0);
}
