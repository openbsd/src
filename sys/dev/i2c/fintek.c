/*	$OpenBSD: fintek.c,v 1.1 2006/06/15 20:50:44 drahn Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@openbsd.org>
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

/* Sensors */
#define F_VCC	0
#define F_V1	1
#define F_V2	2
#define F_V3	3
#define F_TEMP1	4
#define F_TEMP2	5
#define F_FAN1	6
#define F_FAN2	7
#define F_NUM_SENSORS	8


struct fintek_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	struct sensor sc_sensor[F_NUM_SENSORS];
};

int	fintek_match(struct device *, void *, void *);
void	fintek_attach(struct device *, struct device *, void *);

void	fintek_refresh(void *);

int	fintek_read_reg(struct fintek_softc *sc, u_int8_t cmd, u_int8_t *data,
	    size_t size);
int	fintek_write_reg(struct fintek_softc *sc, u_int8_t cmd, u_int8_t *data,
	    size_t size);

void fintek_setspeed(struct fintek_softc *sc);
void fintek_setauto(struct fintek_softc *sc);
void fintek_setpwm(struct fintek_softc *sc);

struct cfattach fintek_ca = {
	sizeof(struct fintek_softc), fintek_match, fintek_attach
};

struct cfdriver fintek_cd = {
	NULL, "fintek", DV_DULL
};

#define FINTEK_VOLT0	0x10
#define FINTEK_VOLT1	0x11
#define FINTEK_VOLT2	0x12
#define FINTEK_VOLT3	0x13
#define FINTEK_TEMP1	0x14
#define FINTEK_TEMP2	0x15
#define FINTEK_FAN1	0x16
#define FINTEK_FAN2	0x18
#define FINTEK_VERSION	0x5c
#define FINTEK_RSTCR	0x60

int
fintek_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "f75375") == 0)
		return (1);
	return (0);
}

int
fintek_read_reg(struct fintek_softc *sc, u_int8_t cmd, u_int8_t *data,
    size_t size)
{
	return iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, data, size, 0);
}
int
fintek_write_reg(struct fintek_softc *sc, u_int8_t cmd, u_int8_t *data,
    size_t size)
{
	return iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, data, size, 0);
}

void
fintek_attach(struct device *parent, struct device *self, void *aux)
{
	struct fintek_softc *sc = (struct fintek_softc *)self;
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data, data2;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = FINTEK_VERSION;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;


	printf(": F75375 rev %d.%d", data>> 4, data & 0xf);


	cmd = FINTEK_RSTCR;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
#if 1
	data = 0x81;
	cmd = 0;
	if (fintek_write_reg(sc, cmd, &data, sizeof data))
		goto failwrite;
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read ID register\n");
		return;
	}
#endif


#ifdef NOISY_DEBUG
	cmd = 0;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" conf 0 %x", data);

	cmd = 0x1;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" conf 1 %x", data);

	cmd = 0x2;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;

	printf(" conf 2 %x", data);
	cmd = 0x3;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" conf 3 %x\n", data);
	cmd = 0x70;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0x71;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data2))
		goto failread;
	printf(" fan full speed %x %x\n", data, data2);

	cmd = 0xa0;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" temp 1 b %x\n", data);

	cmd = 0xa1;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" temp 2 b %x\n", data);

	cmd = 0xa2;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" temp 3 b %x\n", data);

	cmd = 0xa3;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	printf(" temp 4 b %x\n", data);

	cmd = 0xa4;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0xa5;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data))
		goto failread;
	printf(" sec1speed %x %x\n", data, data2);

	cmd = 0xa6;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0xa7;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data))
		goto failread;
	printf(" sec2speed %x %x\n", data, data2);

	cmd = 0xa8;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0xa9;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data))
		goto failread;
	printf(" sec3speed %x %x\n", data, data2);

	cmd = 0xaa;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0xab;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data))
		goto failread;
	printf(" sec4speed %x %x\n", data, data2);

	cmd = 0xac;
	if (fintek_read_reg(sc, cmd, &data, sizeof data))
		goto failread;
	cmd = 0xad;
	if (fintek_read_reg(sc, cmd, &data2, sizeof data))
		goto failread;
	printf(" sec5speed %x %x\n", data, data2);
#endif

#if 0
	fintek_setpwm(sc);
#endif
#if 0
	fintek_setauto(sc);
#endif
	data2 = data2;
#if 1
	fintek_setspeed(sc);
#endif
	iic_release_bus(sc->sc_tag, 0);

for (i = 0; i < F_NUM_SENSORS; i++)
		strlcpy(sc->sc_sensor[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sc_sensor[i].device));

	sc->sc_sensor[F_VCC].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[F_VCC].desc, "VCC",
	    sizeof(sc->sc_sensor[F_VCC].desc));

	sc->sc_sensor[F_V1].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[F_V1].desc, "Volt 1",
	    sizeof(sc->sc_sensor[F_V1].desc));

	sc->sc_sensor[F_V2].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[F_V2].desc, "Volt 2",
	    sizeof(sc->sc_sensor[F_V2].desc));

	sc->sc_sensor[F_V3].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[F_V3].desc, "Volt 3",
	    sizeof(sc->sc_sensor[F_V3].desc));

	sc->sc_sensor[F_TEMP1].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[F_TEMP1].desc, "Temp 1",
	    sizeof(sc->sc_sensor[F_TEMP1].desc));

	sc->sc_sensor[F_TEMP2].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[F_TEMP2].desc, "Temp 2",
	    sizeof(sc->sc_sensor[F_TEMP2].desc));

	sc->sc_sensor[F_FAN1].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[F_FAN1].desc, "FAN1",
	    sizeof(sc->sc_sensor[F_FAN1].desc));

	sc->sc_sensor[F_FAN2].type = SENSOR_FANRPM;
	strlcpy(sc->sc_sensor[F_FAN2].desc, "FAN1",
	    sizeof(sc->sc_sensor[F_FAN2].desc));

	if (sensor_task_register(sc, fintek_refresh, 5)) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < F_NUM_SENSORS; i++) {
		sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		sensor_add(&sc->sc_sensor[i]);
	}
	return;

failread:
	printf("unable to read reg %d\n", cmd);
	iic_release_bus(sc->sc_tag, 0);
	return;
#if 1
failwrite:
	printf("unable to write reg %d\n", cmd);
	iic_release_bus(sc->sc_tag, 0);
#endif
}


struct {
	char		sensor;
	u_int8_t	cmd;
} fintek_worklist[] = {
	{ F_VCC, FINTEK_VOLT0 },
	{ F_V1, FINTEK_VOLT1 },
	{ F_V2, FINTEK_VOLT2 },
	{ F_V3, FINTEK_VOLT3 },
	{ F_TEMP1, FINTEK_TEMP1 },
	{ F_TEMP2, FINTEK_TEMP2 },
	{ F_FAN1, FINTEK_FAN1 },
	{ F_FAN2, FINTEK_FAN2 }
};
#define FINTEK_WORKLIST_SZ (sizeof fintek_worklist/sizeof(fintek_worklist[0]))

void
fintek_refresh(void *arg)
{
	struct fintek_softc *sc =  arg;
	u_int8_t cmd, data, data2;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	for (i = 0; i < FINTEK_WORKLIST_SZ; i++){
		cmd = fintek_worklist[i].cmd;
		if (fintek_read_reg(sc, cmd, &data, sizeof data)) {
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			continue;
		}
		sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		switch (fintek_worklist[i].sensor) {
		case  F_VCC:
			/* FALLTHROUGH */
		case  F_V1:
			/* FALLTHROUGH */
		case  F_V2:
			/* FALLTHROUGH */
		case  F_V3:
			sc->sc_sensor[i].value = 1000 * (data*8);
			break;
		case  F_TEMP1:
			/* FALLTHROUGH */
		case  F_TEMP2:
			sc->sc_sensor[i].value = 273150000 + 1000000 * (data);
			break;
		case  F_FAN1:
			/* FALLTHROUGH */
		case  F_FAN2:
			/* FANxLSB follows FANxMSB */
			cmd = fintek_worklist[i].cmd + 1;
			if (fintek_read_reg(sc, cmd, &data2, sizeof data2)) {
				sc->sc_sensor[i].flags |= SENSOR_FINVALID;
				continue;
			}
//			printf("fan speed %x: %x %x\n", fintek_worklist[i].cmd,
//			    data, data2);
			if ((data == 0xff && data2 == 0xff) ||
			    (data == 0 && data2 == 0))
				sc->sc_sensor[i].value = 0;
			else
				sc->sc_sensor[i].value = 1500000/
				    (data << 8 | data2);
			{
				extern long hostid;
				static long currentspeed;
				int i;
				if (currentspeed != hostid) {
					currentspeed = hostid;
					printf("setting speed to %d\n", hostid);
 
#if 0
					data = hostid & 0xff;
					cmd = 0x76;
					fintek_write_reg(sc, cmd, &data,
					    sizeof data);
#else
					cmd = 0x6d;
					fintek_read_reg(sc, cmd, &data,
					    sizeof data);
					printf("reg 6d contains %x setting to 0x11\n", data);
					fintek_write_reg(sc, cmd, &data,
					    sizeof data);
					i = hostid ; /* desired value */
					cmd = 0x74;
					data = i >> 8;
					fintek_write_reg(sc, cmd, &data,
					    sizeof data);

					cmd = 0x75;
					data = i & 0xff;
					fintek_write_reg(sc, cmd, &data,
					    sizeof data);
#endif
				}
			}
			break;
		default:
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			break;
		}
	}

	iic_release_bus(sc->sc_tag, 0);
}
void
fintek_setspeed(struct fintek_softc *sc)
{
	u_int8_t cmd, data;
	int i;

	cmd = 0x1;
	fintek_read_reg(sc, cmd, &data, sizeof data);

	data |= (1<<4);
	fintek_write_reg(sc, cmd, &data, sizeof data);

	i = 300; /* desired speed */
	i = 1500000/i;
	cmd = 0x74;
	data = i >> 8;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0x75;
	data = i & 0xff;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0x60;
	fintek_read_reg(sc, cmd, &data, sizeof data);

	data &= ~(0x1 << 4);
	data |= (0x2 << 4); /* manual */
	fintek_write_reg(sc, cmd, &data, sizeof data);
	printf("\n");
}	

void
fintek_setauto(struct fintek_softc *sc)
{
	u_int8_t cmd, data, data2;
	int i;

	i = 2000; /* desired speed */
	i = 1500000/i;
	data2 = i >> 8;
	data = i & 0xff;

	cmd = 0xa4;
	fintek_write_reg(sc, cmd, &data, sizeof data);
	fintek_write_reg(sc, cmd, &data2, sizeof data2);
	
	i = 0x3A98;
	data2 = i >> 8;
	data = i & 0xff;
	cmd = 0xa6;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0xa7;
	fintek_write_reg(sc, cmd, &data2, sizeof data2);

	i = 0x3A98;
	data2 = i >> 8;
	data = i & 0xff;
	cmd = 0xa8;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0xa9;
	fintek_write_reg(sc, cmd, &data2, sizeof data2);

	i = 0x3A98;
	data2 = i >> 8;
	data = i & 0xff;
	cmd = 0xaa;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0xab;
	fintek_write_reg(sc, cmd, &data2, sizeof data2);

	i = 0x3A98;
	data2 = i >> 8;
	data = i & 0xff;
	cmd = 0xac;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0xad;
	fintek_write_reg(sc, cmd, &data2, sizeof data2);

	cmd = 0x74;
	fintek_read_reg(sc, cmd, &data, sizeof data);

	cmd = 0x75;
	fintek_read_reg(sc, cmd, &data2, sizeof data);

	printf("fan speed %x %x\n", data,  data2);

	cmd = 0x60;
	fintek_read_reg(sc, cmd, &data, sizeof data);

	data &= ~(0x3 << 4);
	data |= (1 << 4);
	fintek_write_reg(sc, cmd, &data, sizeof data);
	
	printf("\n");

}
void
fintek_setpwm(struct fintek_softc *sc)
{
	u_int8_t cmd, data;

	cmd = 0x01;
	fintek_read_reg(sc, cmd, &data, sizeof data);
	data |= 1<<4;
	fintek_write_reg(sc, cmd, &data, sizeof data);

	cmd = 0x60;
	fintek_read_reg(sc, cmd, &data, sizeof data);

	data |= 0x3 << 4;

	fintek_write_reg(sc, cmd, &data, sizeof data);


	data = 0x28;
	cmd = 0x76;
	fintek_write_reg(sc, cmd, &data, sizeof data);
	
	printf("\n");

}
