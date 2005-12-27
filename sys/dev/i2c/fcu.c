/*	$OpenBSD: fcu.c,v 1.2 2005/12/27 17:18:18 deraadt Exp $	*/

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

/* FCU registers */
#define FCU_FAN_FAIL	0x0b		/* fans states in bits 0<1-6>7 */
#define FCU_FAN_ACTIVE	0x0d
#define FCU_FANREAD(x)	0x11 + (x)*2
#define FCU_FANSET(x)	0x10 + (x)*2
#define FCU_PWM_FAIL	0x2b
#define FCU_PWM_ACTIVE	0x2d
#define FCU_PWMREAD(x)	0x30 + (x)*2

/* Sensors */
#define FCU_RPM1		0
#define FCU_RPM2		1
#define FCU_RPM3		2
#define FCU_RPM4		3
#define FCU_RPM5		4
#define FCU_RPM6		5
#define  FCU_FANS		6
#define FCU_PWM1		6
#define FCU_PWM2		7
#define  FCU_PWMS		2
#define FCU_NUM_SENSORS		8

struct fcu_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct sensor sc_sensor[FCU_NUM_SENSORS];
};

int	fcu_match(struct device *, void *, void *);
void	fcu_attach(struct device *, struct device *, void *);

void	fcu_refresh(void *);

struct cfattach fcu_ca = {
	sizeof(struct fcu_softc), fcu_match, fcu_attach
};

struct cfdriver fcu_cd = {
	NULL, "fcu", DV_DULL
};

int
fcu_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "fcu") == 0)
		return (1);
	return (0);
}

void
fcu_attach(struct device *parent, struct device *self, void *aux)
{
	struct fcu_softc *sc = (struct fcu_softc *)self;
	struct i2c_attach_args *ia = aux;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/* Initialize sensor data. */
	for (i = 0; i < FCU_FANS; i++) {
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));
		sc->sc_sensor[i].type = SENSOR_FANRPM;
		snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc), "FAN%d", i);
	}
	for (i = 0; i < FCU_PWMS; i++) {
		strlcpy(sc->sc_sensor[FCU_PWM1 + i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[FCU_PWM1 + i].device));
		sc->sc_sensor[FCU_PWM1 + i].type = SENSOR_FANRPM;
		snprintf(sc->sc_sensor[FCU_PWM1 + i].desc,
		    sizeof(sc->sc_sensor[FCU_PWM1 + i].desc), "PWM%d", i);
	}

	if (sensor_task_register(sc, fcu_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < FCU_NUM_SENSORS; i++)
		SENSOR_ADD(&sc->sc_sensor[i]);

	printf("\n");
}

void
fcu_refresh(void *arg)
{
	struct fcu_softc *sc = arg;
	u_int8_t cmd, fail, fan[2], active;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = FCU_FAN_FAIL;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &fail, sizeof fail, 0))
		goto abort;
	cmd = FCU_FAN_ACTIVE;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &active, sizeof active, 0))
		goto abort;
	fail &= active;

	for (i = 0; i < FCU_FANS; i++) {
		if (fail & (1 << (i + 1)))
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
		else
			sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
	}

	cmd = FCU_PWM_FAIL;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &fail, sizeof fail, 0))
		goto abort;
	cmd = FCU_PWM_ACTIVE;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &active, sizeof active, 0))
		goto abort;
	fail &= active;

	for (i = 0; i < FCU_PWMS; i++) {
		if (fail & (1 << (i + 1)))
			sc->sc_sensor[FCU_PWMS + i].flags |= SENSOR_FINVALID;
		else
			sc->sc_sensor[FCU_PWMS + i].flags &= ~SENSOR_FINVALID;
	}

	for (i = 0; i < FCU_FANS; i++) {
		cmd = FCU_FANREAD(i + 1);
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &fan, sizeof fan, 0)) {
			sc->sc_sensor[FCU_RPM1 + i].flags |= SENSOR_FINVALID;
			continue;
		}
		sc->sc_sensor[FCU_RPM1 + i].value = (fan[0] << 5) | (fan[1] >> 3);
	}

	for (i = 0; i < FCU_PWMS; i++) {
		cmd = FCU_PWMREAD(i + 1);
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &fan, sizeof fan, 0)) {
			sc->sc_sensor[FCU_PWM1 + i].flags |= SENSOR_FINVALID;
			continue;
		}
		sc->sc_sensor[FCU_PWM1 + i].value = (fan[0] * 1000) / 2559;
	}

abort:
	iic_release_bus(sc->sc_tag, 0);
}
