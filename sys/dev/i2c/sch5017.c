/*
 * Copyright (c) 2006 Roman Hunt
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

/*
 * SCH5017 Registers
 */
#define SCH5017_VCCP		0x21
#define SCH5017_VCC		0x22
#define SCH5017_5V		0x23
#define SCH5017_12V		0x24
#define SCH5017_RTEMP1		0x25
#define SCH5017_ITEMP		0x26
#define SCH5017_RTEMP2		0x27
#define SCH5017_FAN1_LSB	0x28
#define SCH5017_FAN1_MSB	0x29
#define SCH5017_FAN2_LSB	0x2a
#define SCH5017_FAN2_MSB	0x2b
#define SCH5017_FAN3_LSB	0x2c
#define SCH5017_FAN3_MSB	0x2d
#define SCH5017_FAN4_LSB	0x2e
#define SCH5017_FAN4_MSB	0x2f
#define SCH5017_VERSION		0x3f

/*
 * Sensors
 */
#define SCHENV_VCCP		0
#define SCHENV_VCC		1
#define SCHENV_5V		2
#define SCHENV_12V		3
#define SCHENV_RTEMP1		4
#define SCHENV_ITEMP		5
#define SCHENV_RTEMP2		6
#define SCHENV_FAN1		7
#define SCHENV_FAN2		8
#define SCHENV_FAN3		9
#define SCHENV_FAN4		10
#define SCHENV_SENSOR_COUNT	11

struct schenv_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct sensor sc_sensor[SCHENV_SENSOR_COUNT];
};

int	schenv_match(struct device *, void *, void *);
void	schenv_attach(struct device *, struct device *, void *);

void	schenv_refresh(void *);

struct cfattach schenv_ca = {
	sizeof(struct schenv_softc), schenv_match, schenv_attach
};

struct cfdriver schenv_cd = {
	NULL, "schenv", DV_DULL
};

int
schenv_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "sch5017") == 0)
		return (1);
	return (0);
}

void
schenv_attach(struct device *parent, struct device *self, void *aux)
{
	struct schenv_softc *sc = (struct schenv_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = SCH5017_VERSION;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read revision register\n");
		return;
	}
	printf(": %s rev %x", ia->ia_name, (data >> 4));

	iic_release_bus(sc->sc_tag, 0);

	/*
	 * Initialize sensors
	 */
	for (i = 0; i < SCHENV_SENSOR_COUNT; i++)
	    strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[SCHENV_VCCP].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[SCHENV_VCCP].desc, "Vccp",
	    sizeof(sc->sc_sensor[SCHENV_VCCP].desc));

	sc->sc_sensor[SCHENV_VCC].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[SCHENV_VCC].desc, "Vcc",
	    sizeof(sc->sc_sensor[SCHENV_VCC].desc));

	sc->sc_sensor[SCHENV_5V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[SCHENV_5V].desc, "+5Vin",
	    sizeof(sc->sc_sensor[SCHENV_5V].desc));

	sc->sc_sensor[SCHENV_12V].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[SCHENV_12V].desc, "+12Vin",
	    sizeof(sc->sc_sensor[SCHENV_12V].desc));

	sc->sc_sensor[SCHENV_RTEMP1].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[SCHENV_RTEMP1].desc, "Ext. Temp. 1",
	    sizeof(sc->sc_sensor[SCHENV_RTEMP1].desc));

	sc->sc_sensor[SCHENV_ITEMP].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[SCHENV_ITEMP].desc, "Int. Temp.",
	    sizeof(sc->sc_sensor[SCHENV_ITEMP].desc));

	sc->sc_sensor[SCHENV_RTEMP2].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[SCHENV_RTEMP2].desc, "Ext. Temp. 2",
	    sizeof(sc->sc_sensor[SCHENV_RTEMP1].desc));

	sc->sc_sensor[SCHENV_FAN1].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[SCHENV_FAN1].desc, "FAN1",
	    sizeof(sc->sc_sensor[SCHENV_FAN1].desc));

	sc->sc_sensor[SCHENV_FAN2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[SCHENV_FAN2].desc, "FAN2",
	    sizeof(sc->sc_sensor[SCHENV_FAN2].desc));

	sc->sc_sensor[SCHENV_FAN3].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[SCHENV_FAN3].desc, "FAN3",
	    sizeof(sc->sc_sensor[SCHENV_FAN3].desc));

	sc->sc_sensor[SCHENV_FAN4].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[SCHENV_FAN4].desc, "FAN4",
	    sizeof(sc->sc_sensor[SCHENV_FAN4].desc));

	if (sensor_task_register(sc, schenv_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < SCHENV_SENSOR_COUNT; i++)
		sensor_add(&sc->sc_sensor[i]);

	printf("\n");
}

void
schenv_refresh(void *arg)
{
	struct schenv_softc *sc = arg;
	u_int8_t cmd, data, data2;
	u_int16_t fanword;
	int sensor;
	iic_acquire_bus(sc->sc_tag, 0);

	for (sensor = 0; sensor < SCHENV_SENSOR_COUNT; sensor++) {
		cmd = SCH5017_VCCP + sensor;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
			continue;
		}

		sc->sc_sensor[sensor].flags &= ~SENSOR_FINVALID;
		switch (sensor) {
		case SCHENV_VCCP:
			sc->sc_sensor[sensor].value = 3000000 / 256 * data;
			break;
		case SCHENV_VCC:
			sc->sc_sensor[sensor].value = 4380000 / 256 * data;
			break;
		case SCHENV_5V:
			sc->sc_sensor[sensor].value = 6640000 / 256 * data;
			break;
		case SCHENV_12V:
			sc->sc_sensor[sensor].value = 16000000 / 256 * data;
			break;
		case SCHENV_RTEMP1:
			/* FALLTHROUGH */
		case SCHENV_ITEMP:
			/* FALLTHROUGH */
		case SCHENV_RTEMP2:
			if (data == 0x80) {
				sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
				break;
			}

			sc->sc_sensor[sensor].value =
			    (int8_t)data * 1000000 + 273150000;

			break;
		case SCHENV_FAN1:
			cmd = SCH5017_FAN1_LSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data,
			    sizeof data, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}

			cmd = SCH5017_FAN1_MSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data2,
			    sizeof data2, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}
			fanword = data2;
			fanword = fanword << 8;
			fanword |= data;
			if (fanword == 0xFFFF) {
				sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
				break;
			}
			sc->sc_sensor[sensor].value = fanword;
			break;
		case SCHENV_FAN2:
			cmd = SCH5017_FAN2_LSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data,
			    sizeof data, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}

			cmd = SCH5017_FAN2_MSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data2,
			    sizeof data2, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}
			fanword = data2;
			fanword = fanword << 8;
			fanword |= data;
			if (fanword == 0xFFFF) {
				sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
				break;
			}
			sc->sc_sensor[sensor].value = fanword;
			break;
		case SCHENV_FAN3:
			cmd = SCH5017_FAN3_LSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data,
			    sizeof data, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}

			cmd = SCH5017_FAN3_MSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data2,
			    sizeof data2, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}
			fanword = data2;
			fanword = fanword << 8;
			fanword |= data;
			if (fanword == 0xFFFF) {
				sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
				break;
			}
			sc->sc_sensor[sensor].value = fanword;
			break;
		case SCHENV_FAN4:
			cmd = SCH5017_FAN4_LSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data,
			    sizeof data, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}

			cmd = SCH5017_FAN4_MSB;
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &data2,
			    sizeof data2, 0)) {
				sc->sc_sensor[sensor].flags |=
				    SENSOR_FINVALID;
				break;
			}
			fanword = data2;
			fanword = fanword << 8;
			fanword |= data;
			if (fanword == 0xFFFF) {
				sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
				break;
			}
			sc->sc_sensor[sensor].value = fanword;
			break;
		default:
			sc->sc_sensor[sensor].flags |= SENSOR_FINVALID;
			break;
		}
	}
	iic_release_bus(sc->sc_tag, 0);
}
