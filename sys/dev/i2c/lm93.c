/*	$OpenBSD: lm93.c,v 1.1 2007/09/04 20:19:22 deraadt Exp $	*/

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

/*
 * iic0: addr 0x2e
 * 3e=01 3f=73 50=26 51=28 52=16 54=25 55=27 56=c4 57=c3 58=c1 59=bf 5a=bf
 * 5b=bf 5c=d1 5d=d3 5e=c3 5f=c3 60=8c 61=fa 62=f6 63=bd 64=31 65=c4 6b=09
 * 6e=04 6f=03 70=d4 71=03 72=88 73=03 74=fc 78=80 79=80 7a=80 7b=80 7c=80
 * 7d=32 7e=80 7f=80 80=50 81=50 82=7f 83=7f 90=ac 91=d3 92=ac 93=d3 94=ac
 * 95=d3 96=ac 97=d3 98=ac 99=d3 9a=ac 9b=d3 9c=a3 9d=f6 9e=a3 9f=f6 a0=ac
 * a1=d3 a2=ac a3=d3 a4=ac a5=d3 a6=ac a7=d3 a8=ac a9=d3 aa=ac ab=d3 ac=0f
 * ad=70 ae=ac af=d3 b2=17 b3=17 b4=a0 b5=1d b6=a0 b7=1d b8=a0 b9=1d ba=fc
 * c0=55 c1=44 c3=3a c7=11 c8=0c c9=02 cc=03 cd=30 d0=37 d1=37 d2=7f d3=7f
 * d6=0a db=0a e3=81 e4=02 e6=0f e8=08 e9=07 eb=07 ed=3f f0=91 f1=91 f2=08
 * f3=08 f4=06 f5=10 f6=04 f7=08 f8=0c f9=20 fa=08 fb=08 fc=10 fd=09 fe=91
 * ff=09
*/

/* LM93 registers */
#define LM93_CPU1_TEMP		0x50
#define LM93_CPU2_TEMP		0x51
#define LM93_INT_TEMP		0x52
#define LM93_EXT_TEMP		0x53
#define LM93_IN1_V		0x56
#define LM93_IN2_V		0x57
#define LM93_IN3_V		0x58
#define LM93_IN4_V		0x59
#define LM93_IN5_V		0x5a
#define LM93_IN6_V		0x5b
#define LM93_IN7_V		0x5c
#define LM93_IN8_V		0x5d
#define LM93_IN9_V		0x5e
#define LM93_IN10_V		0x5f
#define LM93_IN11_V		0x60
#define LM93_IN12_V		0x61
#define LM93_IN13_V		0x62
#define LM93_IN14_V		0x63
#define LM93_IN15_V		0x64
#define LM93_IN16_V		0x65
#define LM93_TACH1L		0x6e
#define LM93_TACH1H		0x6f
#define LM93_TACH2L		0x70
#define LM93_TACH2H		0x71
#define LM93_TACH3L		0x72
#define LM93_TACH3H		0x73
#define LM93_TACH4L		0x74
#define LM93_TACH4H		0x75
#define LM93_REVISION		0x3f

/* Sensors */
#define LMN_CPU1_TEMP		0
#define LMN_CPU2_TEMP		1
#define LMN_INT_TEMP		2
#define LMN_EXT_TEMP		3
#define LMN_IN1_V		4
#define LMN_IN2_V		5
#define LMN_IN3_V		6
#define LMN_IN4_V		7
#define LMN_IN5_V		8
#define LMN_IN6_V		9
#define LMN_IN7_V		10
#define LMN_IN8_V		11
#define LMN_IN9_V		12
#define LMN_IN10_V		13
#define LMN_IN11_V		14
#define LMN_IN12_V		15
#define LMN_IN13_V		16
#define LMN_IN14_V		17
#define LMN_IN15_V		18
#define LMN_IN16_V		19
#define LMN_TACH1		20
#define LMN_TACH2		21
#define LMN_TACH3		22
#define LMN_TACH4		23
#define LMN_NUM_SENSORS		24

struct {
	char		sensor;
	u_int8_t	cmd;
	char		*name;
	u_short		mVscale;
	u_short		tempscale;		/* else a fan */
} lmn_worklist[] = {
	{ LMN_CPU1_TEMP, LM93_CPU1_TEMP, "CPU1", 0, 1 },
	{ LMN_CPU2_TEMP, LM93_CPU2_TEMP, "CPU2", 0, 1 },
	{ LMN_INT_TEMP, LM93_INT_TEMP, "Internal", 0, 1 },
	{ LMN_EXT_TEMP, LM93_EXT_TEMP, "External", 0, 1 },

	{ LMN_IN1_V, LM93_IN1_V, "+12V1", 1236*10, 0 },
	{ LMN_IN2_V, LM93_IN2_V, "+12V2", 1236*10, 0 },
	{ LMN_IN3_V, LM93_IN3_V, "+12V3", 1236*10, 0 },
	{ LMN_IN4_V, LM93_IN4_V, "FSB_Vtt 1.6V", 1600, 0 },
	{ LMN_IN5_V, LM93_IN5_V, "3GIO 2V ", 2000, 0 },
	{ LMN_IN6_V, LM93_IN6_V, "ICH_Core 2V", 2000, 0 },
	{ LMN_IN7_V, LM93_IN7_V, "Vccp1 1.6V", 1600, 0,},
	{ LMN_IN8_V, LM93_IN8_V, "Vccp2 1.6V", 1600, 0 },
	{ LMN_IN9_V, LM93_IN9_V, "+3.3V", 4400, 0 },
	{ LMN_IN10_V, LM93_IN10_V, "+5V", 6667, 0 },
	{ LMN_IN11_V, LM93_IN11_V, "SCSI_Core 3.3V", 3333, 0 },
	{ LMN_IN12_V, LM93_IN12_V, "Mem_Core 2.6V", 2625, 0 },
	{ LMN_IN13_V, LM93_IN13_V, "Mem_Vtt 1.3V", 1312, 0 },
	{ LMN_IN14_V, LM93_IN14_V, "Gbit_Core 1.3V", 1312, 0 },
	{ LMN_IN15_V, LM93_IN15_V, "-12V", 1236*10, 0 },
	{ LMN_IN16_V, LM93_IN16_V, "+3.3V S/B", 3600, 0 },

	{ LMN_TACH1, LM93_TACH1L, "tach1", 0, 0 },
	{ LMN_TACH2, LM93_TACH2L, "tach2", 0, 0 },
	{ LMN_TACH3, LM93_TACH3L, "tach3", 0, 0 },
	{ LMN_TACH4, LM93_TACH4L, "tach4", 0, 0 }
};

struct lmn_softc {
	struct device sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;
	u_int8_t sc_conf;

	struct ksensor sc_sensor[LMN_NUM_SENSORS];
	struct ksensordev sc_sensordev;
};

int	lmn_match(struct device *, void *, void *);
void	lmn_attach(struct device *, struct device *, void *);

void	lmn_refresh(void *);

struct cfattach lmn_ca = {
	sizeof(struct lmn_softc), lmn_match, lmn_attach
};

struct cfdriver lmn_cd = {
	NULL, "lmn", DV_DULL
};

int
lmn_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
lmn_attach(struct device *parent, struct device *self, void *aux)
{
	struct lmn_softc *sc = (struct lmn_softc *)self;
	struct i2c_attach_args *ia = aux;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	printf(": %s", ia->ia_name);

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	if (sensor_task_register(sc, lmn_refresh, 5) == NULL) {
		printf(", unable to register update task\n");
		return;
	}

	for (i = 0; i < LMN_NUM_SENSORS; i++) {
		if (lmn_worklist[i].tempscale)
			sc->sc_sensor[i].type = SENSOR_TEMP;
		else if (lmn_worklist[i].mVscale)
			sc->sc_sensor[i].type = SENSOR_VOLTS_DC;
		else
			sc->sc_sensor[i].type = SENSOR_FANRPM;
		strlcpy(sc->sc_sensor[i].desc, lmn_worklist[i].name,
		    sizeof(sc->sc_sensor[i].desc));

		sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	}
	sensordev_install(&sc->sc_sensordev);

	printf("\n");
}

void
lmn_refresh(void *arg)
{
	struct lmn_softc *sc = arg;
	u_int8_t cmd, data, data2;
	u_int16_t fan;
	int i;

	iic_acquire_bus(sc->sc_tag, 0);

	for (i = 0; i < sizeof lmn_worklist / sizeof(lmn_worklist[0]); i++) {

		cmd = lmn_worklist[i].cmd;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			continue;
		}

		sc->sc_sensor[i].flags &= ~SENSOR_FINVALID;
		if (lmn_worklist[i].tempscale) {
			if (data == 0x80)
				sc->sc_sensor[i].flags |= SENSOR_FINVALID;
			else
				sc->sc_sensor[i].value =
				    (int8_t)data * 1000000 + 273150000;
		} else if (lmn_worklist[i].mVscale) {
			sc->sc_sensor[i].value = lmn_worklist[i].mVscale *
			    1000 * (u_int)data / 192;
		} else {
			cmd = lmn_worklist[i].cmd + 1; /* TACHnH follows TACHnL */
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
		}
	}

	iic_release_bus(sc->sc_tag, 0);
}
