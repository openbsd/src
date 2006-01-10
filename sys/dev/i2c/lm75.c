/*	$OpenBSD: lm75.c,v 1.10 2006/01/10 23:02:32 deraadt Exp $	*/
/*	$NetBSD: lm75.c,v 1.1 2003/09/30 00:35:31 thorpej Exp $	*/
/*
 * Copyright (c) 2006 Theo de Raadt <deraadt@openbsd.org>
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

/*
 * National Semiconductor LM75/LM77 temperature sensor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sensors.h>

#include <dev/i2c/i2cvar.h>

#define	LM_MODEL_LM75	1
#define	LM_MODEL_LM77	2
#define	LM_MODEL_DS1775	3

#define LM_POLLTIME	3	/* 3s */

#define	LM75_REG_TEMP			0x00
#define	LM75_REG_CONFIG			0x01
#define  LM75_CONFIG_SHUTDOWN		0x01
#define  LM75_CONFIG_CMPINT		0x02
#define  LM75_CONFIG_OSPOLARITY		0x04
#define  LM75_CONFIG_FAULT_QUEUE_MASK	0x18
#define  LM75_CONFIG_FAULT_QUEUE_1	(0 << 3)
#define  LM75_CONFIG_FAULT_QUEUE_2	(1 << 3)
#define  LM75_CONFIG_FAULT_QUEUE_4	(2 << 3)
#define  LM75_CONFIG_FAULT_QUEUE_6	(3 << 3)
#define  LM77_CONFIG_INTPOLARITY	0x08
#define  LM77_CONFIG_FAULT_QUEUE_4	0x10
#define  DS1755_CONFIG_RESOLUTION(i)	(9 + (((i) >> 5) & 3))
#define	LM75_REG_THYST_SET_POINT	0x02
#define	LM75_REG_TOS_SET_POINT		0x03
#define	LM77_REG_TLOW			0x04
#define	LM77_REG_THIGH			0x05

struct lmtemp_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	int	sc_addr;
	int	sc_model;
	int	sc_bits;

	struct sensor sc_sensor;
};

int  lmtemp_match(struct device *, void *, void *);
void lmtemp_attach(struct device *, struct device *, void *);

struct cfattach lmtemp_ca = {
	sizeof(struct lmtemp_softc),
	lmtemp_match,
	lmtemp_attach
};

struct cfdriver lmtemp_cd = {
	NULL, "lmtemp", DV_DULL
};

/*
 * Temperature on the LM75 is represented by a 9-bit two's complement
 * integer in steps of 0.5C.  The following examples are taken from
 * the LM75 data sheet:
 *
 *	+125C	0 1111 1010	0x0fa
 *	+25C	0 0011 0010	0x032
 *	+0.5C	0 0000 0001	0x001
 *	0C	0 0000 0000	0x000
 *	-0.5C	1 1111 1111	0x1ff
 *	-25C	1 1100 1110	0x1ce
 *	-55C	1 1001 0010	0x192
 *
 * Temperature on the LM77 is represented by a 10-bit two's complement
 * integer in steps of 0.5C:
 *
 *	+130C	01 0000 0100	0x104
 *	+125C	00 1111 1010	0x0fa
 *	+25C	00 0011 0010	0x032
 *	+0.5C	00 0000 0001	0x001
 *	0C	00 0000 0000	0x000
 *	-0.5C	11 1111 1111	0x3ff
 *	-25C	11 1100 1110	0x3ce
 *	-55C	11 1001 0010	0x392
 *
 * LM75 temperature word:
 *
 * MSB Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0 X X X X X X X
 * 15  14   13   12   11   10   9    8    7    6 5 4 3 2 1 0
 *
 *
 * LM77 temperature word:
 *
 * Sign Sign Sign Sign MSB Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0 Status bits
 * 15   14   13   12   11  10   9    8    7    6    5    4    3    2 1 0
 */

int  lmtemp_temp_read(struct lmtemp_softc *, uint8_t, int *);
void lmtemp_refresh_sensor_data(void *);

int
lmtemp_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "lm75") == 0 ||
	    strcmp(ia->ia_name, "lm77") == 0 ||
	    strcmp(ia->ia_name, "ds1775") == 0)
		return (1);
	return (0);
}

void
lmtemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct lmtemp_softc *sc = (struct lmtemp_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	printf(": %s", ia->ia_name);

	/* If in SHUTDOWN mode, wake it up */
	iic_acquire_bus(sc->sc_tag, 0);
	cmd = LM75_REG_CONFIG;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, 1, &data, 1, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		return;
	}
	if (data & LM75_CONFIG_SHUTDOWN) {
		data &= ~LM75_CONFIG_SHUTDOWN;
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, &cmd, 1, &data, 1, 0)) {
			printf(", cannot wake up\n");
			iic_release_bus(sc->sc_tag, 0);
			return;
		}
		printf(", woken up");
	}
	iic_release_bus(sc->sc_tag, 0);

	sc->sc_model = LM_MODEL_LM75;
	sc->sc_bits = 9;
	if (strcmp(ia->ia_name, "lm77") == 0) {
		sc->sc_model = LM_MODEL_LM77;
		sc->sc_bits = 13;
	} else if (strcmp(ia->ia_name, "ds1775") == 0) {
		sc->sc_model = LM_MODEL_DS1775;
		sc->sc_bits = 9;
		//sc->sc_bits = DS1755_CONFIG_RESOLUTION(data);
	}

	printf("\n");

	/* Initialize sensor data */
	strlcpy(sc->sc_sensor.device, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensor.device));
	sc->sc_sensor.type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor.desc, "TEMP", sizeof(sc->sc_sensor.desc));

	/* Hook into the hw.sensors sysctl */
	SENSOR_ADD(&sc->sc_sensor);

	sensor_task_register(sc, lmtemp_refresh_sensor_data, LM_POLLTIME);
}

int
lmtemp_temp_read(struct lmtemp_softc *sc, uint8_t which, int *valp)
{
	u_int8_t cmd, buf[2];
	int error;

	cmd = which;
	error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, 1, buf, 2, 0);
	if (error)
		return (error);

	/* Some chips return transient 0's.. we try next time */
	if (buf[0] == 0x00 && buf[1] == 0x00)
		return (1);

	/* convert to half-degrees C */
	*valp = ((buf[0] << 8) | buf[1]) / (1 << (16 - sc->sc_bits));
	return (0);
}

void
lmtemp_refresh_sensor_data(void *aux)
{
	struct lmtemp_softc *sc = aux;
	int val;
	int error;

	error = lmtemp_temp_read(sc, LM75_REG_TEMP, &val);
	if (error) {
#if 0
		printf("%s: unable to read temperature, error = %d\n",
		    sc->sc_dev.dv_xname, error);
#endif
		sc->sc_sensor.flags |= SENSOR_FINVALID;
		return;
	}

	sc->sc_sensor.value = val * 500000 + 273150000;
	sc->sc_sensor.flags &= ~SENSOR_FINVALID;
}
