/*	$OpenBSD: adt7462.c,v 1.4 2008/04/22 00:25:32 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Theo de Raadt
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

#define ADT7462_INT_TEMPL	0x88
#define ADT7462_INT_TEMPH	0x89
#define ADT7462_INT_REM1L	0x8a
#define ADT7462_INT_REM1H	0x8b
#define ADT7462_INT_REM2L	0x8c
#define ADT7462_INT_REM2H	0x8d
#define ADT7462_INT_REM3L	0x8e
#define ADT7462_INT_REM3H	0x8f

/* Sensors */
#define ADTFSM_TEMP0		0
#define ADTFSM_TEMP1		1
#define ADTFSM_TEMP2		2
#define ADTFSM_TEMP3		3
#define ADTFSM_NUM_SENSORS	4

struct adtfsm_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;
	int		sc_fanmul;

	struct ksensor	sc_sensor[ADTFSM_NUM_SENSORS];
	struct ksensordev sc_sensordev;
};

int	adtfsm_match(struct device *, void *, void *);
void	adtfsm_attach(struct device *, struct device *, void *);
void	adtfsm_refresh(void *);

struct cfattach adtfsm_ca = {
	sizeof(struct adtfsm_softc), adtfsm_match, adtfsm_attach
};

struct cfdriver adtfsm_cd = {
	NULL, "adtfsm", DV_DULL
};

int
adtfsm_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "adt7462") == 0)
		return (1);
	return (0);
}

void
adtfsm_attach(struct device *parent, struct device *self, void *aux)
{
	struct adtfsm_softc *sc = (struct adtfsm_softc *)self;
	struct i2c_attach_args *ia = aux;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	for (i = 0; i < 4; i++) {
		sc->sc_sensor[i].type = SENSOR_TEMP;
		snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc),
		    "Temperature%d", i);
	}

	if (sensor_task_register(sc, adtfsm_refresh, 5) == NULL) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < ADTFSM_NUM_SENSORS; i++)
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	sensordev_install(&sc->sc_sensordev);

	printf("\n");
}

void
adtfsm_refresh(void *arg)
{
	struct adtfsm_softc *sc = arg;
	u_int8_t cmdh, cmdl, datah = 0x01, datal = 0x02;
	short t;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	for (i = 0; i < 4; i++) {
		cmdl = ADT7462_INT_TEMPL + i * 2;
		cmdh = ADT7462_INT_TEMPH + i * 2;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmdl, sizeof cmdl, &datal,
		    sizeof datal, 0) == 0 &&
		    iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmdh, sizeof cmdh, &datah,
		    sizeof datah, 0) == 0) {
			t = (((datah << 8) | datal) >> 6) - (64 << 2);
			sc->sc_sensor[i].value = 273150000 + t * 250000;
			sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		} else
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
		printf("val %02x %02x\n", datah, datal);
	}

	iic_release_bus(sc->sc_tag, 0);
}
