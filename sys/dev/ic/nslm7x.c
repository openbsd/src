/*	$OpenBSD: nslm7x.c,v 1.16 2006/01/12 20:03:14 kettenis Exp $	*/
/*	$NetBSD: nslm7x.c,v 1.17 2002/11/15 14:55:41 ad Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sensors.h>
#include <machine/bus.h>

#include <dev/ic/nslm7xvar.h>

#if defined(LMDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

struct cfdriver lm_cd = {
	NULL, "lm", DV_DULL
};

int  lm_match(struct lm_softc *);
int  wb_match(struct lm_softc *);
int  def_match(struct lm_softc *);

void lm_setup_sensors(struct lm_softc *, struct lm_sensor *);
void lm_refresh(void *);

void lm_refresh_sensor_data(struct lm_softc *);
void lm_refresh_volts(struct lm_softc *, int);
void lm_refresh_nvolts(struct lm_softc *, int);
void lm_refresh_temp(struct lm_softc *, int);
void lm_refresh_fanrpm(struct lm_softc *, int);

void wb_refresh_sensor_data(struct lm_softc *);
void wb_refresh_n12volts(struct lm_softc *, int);
void wb_refresh_n5volts(struct lm_softc *, int);
void wb_refresh_temp(struct lm_softc *, int);
void wb_refresh_fanrpm(struct lm_softc *, int);

struct lm_chip {
	int (*chip_match)(struct lm_softc *);
};

struct lm_chip lm_chips[] = {
	{ wb_match },
	{ lm_match },
	{ def_match } /* Must be last */
};

struct lm_sensor lm78_sensors[] = {
	/* Voltage */
	{ "VCore A", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VCore B", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 40000 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, lm_refresh_nvolts, 40000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, lm_refresh_nvolts, 16667 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, lm_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, lm_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, lm_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83627hf_sensors[] = {
	/* Voltage */
	{ "VCore A", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VCore B", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },
	{ "5VSB", SENSOR_VOLTS_DC, 5, 0x50, lm_refresh_volts, 15151 },
	{ "VBAT", SENSOR_VOLTS_DC, 5, 0x51, lm_refresh_volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 2, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83637hf_sensors[] = {
	/* Voltage */
	{ "VCore", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 38000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16667 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x24, wb_refresh_n12volts, 10000 },
	{ "5VSB", SENSOR_VOLTS_DC, 5, 0x50, lm_refresh_volts, 16667 },
	{ "VBAT", SENSOR_VOLTS_DC, 5, 0x51, lm_refresh_volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 2, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83697hf_sensors[] = {
	/* Voltage */
	{ "VCore", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V",SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },
	{ "5VSB", SENSOR_VOLTS_DC, 5, 0x50, lm_refresh_volts, 15151 },
	{ "VBAT", SENSOR_VOLTS_DC, 5, 0x51, lm_refresh_volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83781d_sensors[] = {
	/* Voltage */
	{ "VCore A", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VCore B", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 15050 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000},
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, lm_refresh_nvolts, 34768 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, lm_refresh_nvolts, 15050 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 2, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, lm_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, lm_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, lm_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83782d_sensors[] = {
	/* Voltage */
	{ "VCore A", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VINR0", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000},
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },
	{ "5VSB", SENSOR_VOLTS_DC, 5, 0x50, lm_refresh_volts, 15151 },
	{ "VBAT", SENSOR_VOLTS_DC, 5, 0x51, lm_refresh_volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 2, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83783s_sensors[] = {
	/* Voltage */
	{ "VCore", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor w83791d_sensors[] = {
	/* Voltage */
	{ "VCore", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VINR0", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000 },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000},
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },
	{ "5VSB", SENSOR_VOLTS_DC, 0, 0xb0, lm_refresh_volts, 15151 },
	{ "VBAT", SENSOR_VOLTS_DC, 0, 0xb1, lm_refresh_volts, 10000 },
	{ "VINR1", SENSOR_VOLTS_DC, 0, 0xb2, lm_refresh_volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 0, 0xc0, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 0, 0xc8, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, wb_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, wb_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, wb_refresh_fanrpm },
	{ "Fan4", SENSOR_FANRPM, 0, 0xba, wb_refresh_fanrpm },
	{ "Fan5", SENSOR_FANRPM, 0, 0xbb, wb_refresh_fanrpm },

	{ NULL }
};

struct lm_sensor as99127f_sensors[] = {
	/* Voltage */
	{ "VCore A", SENSOR_VOLTS_DC, 0, 0x20, lm_refresh_volts, 10000 },
	{ "VCore B", SENSOR_VOLTS_DC, 0, 0x21, lm_refresh_volts, 10000 },
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x22, lm_refresh_volts, 10000  },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x23, lm_refresh_volts, 16800 },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x24, lm_refresh_volts, 38000 },
	{ "-12V", SENSOR_VOLTS_DC, 0, 0x25, wb_refresh_n12volts, 10000 },
	{ "-5V", SENSOR_VOLTS_DC, 0, 0x26, wb_refresh_n5volts, 10000 },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0, 0x27, lm_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 1, 0x50, wb_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 2, 0x50, wb_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x28, lm_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x29, lm_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x2a, lm_refresh_fanrpm },

	{ NULL }
};

/* XXX Not so bus-independent probe. */
int
lm_probe(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	u_int8_t cr;
	int rv;

	/* Check for some power-on defaults */
	bus_space_write_1(iot, ioh, LMC_ADDR, LMD_CONFIG);

	/* Perform LM78 reset */
	bus_space_write_1(iot, ioh, LMC_DATA, 0x80);

	/* XXX - Why do I have to reselect the register? */
	bus_space_write_1(iot, ioh, LMC_ADDR, LMD_CONFIG);
	cr = bus_space_read_1(iot, ioh, LMC_DATA);

	/* XXX - spec says *only* 0x08! */
	if ((cr == 0x08) || (cr == 0x01) || (cr == 0x03))
		rv = 1;
	else
		rv = 0;

	DPRINTF(("lm: rv = %d, cr = %x\n", rv, cr));

	return (rv);
}

void
lm_attach(struct lm_softc *sc)
{
	u_int i, config;

	for (i = 0; i < sizeof(lm_chips) / sizeof(lm_chips[0]); i++)
		if (lm_chips[i].chip_match(sc))
			break;

	if (sensor_task_register(sc, lm_refresh, 5)) {
		printf("%s: unable to register update task\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Start the monitoring loop */
	config = sc->lm_readreg(sc, LMD_CONFIG);
	sc->lm_writereg(sc, LMD_CONFIG, config | 0x01);

	/* Add sensors */
	for (i = 0; i < sc->numsensors; ++i)
		SENSOR_ADD(&sc->sensors[i]);
}

int
lm_match(struct lm_softc *sc)
{
	int chipid;

	/* See if we have an LM78 or LM79. */
	chipid = sc->lm_readreg(sc, LMD_CHIPID) & LM_ID_MASK;
	switch(chipid) {
	case LM_ID_LM78:
		printf(": LM78\n");
		break;
	case LM_ID_LM78J:
		printf(": LM78J\n");
		break;
	case LM_ID_LM79:
		printf(": LM79\n");
		break;
	case LM_ID_LM81:
		printf(": LM81\n");
		break;
	default:
		return 0;
	}

	lm_setup_sensors(sc, lm78_sensors);
	sc->refresh_sensor_data = lm_refresh_sensor_data;
	return 1;
}

int
def_match(struct lm_softc *sc)
{
	int chipid;

	chipid = sc->lm_readreg(sc, LMD_CHIPID) & LM_ID_MASK;
	printf(": unknown chip (ID %d)\n", chipid);

	lm_setup_sensors(sc, lm78_sensors);
	sc->refresh_sensor_data = lm_refresh_sensor_data;
	return 1;
}

int
wb_match(struct lm_softc *sc)
{
	int banksel, vendid, chipid;

	/* Read vendor ID */
	banksel = sc->lm_readreg(sc, WB_BANKSEL);
	sc->lm_writereg(sc, WB_BANKSEL, WB_BANKSEL_HBAC);
	vendid = sc->lm_readreg(sc, WB_VENDID) << 8;
	sc->lm_writereg(sc, WB_BANKSEL, 0);
	vendid |= sc->lm_readreg(sc, WB_VENDID);
	sc->lm_writereg(sc, WB_BANKSEL, banksel);
	DPRINTF(("winbond vend id 0x%x\n", j));
	if (vendid != WB_VENDID_WINBOND && vendid != WB_VENDID_ASUS)
		return 0;

	/* Read chip ID */
	sc->lm_writereg(sc, WB_BANKSEL, WB_BANKSEL_B0);
	chipid = sc->lm_readreg(sc, WB_BANK0_CHIPID);
	sc->lm_writereg(sc, WB_BANKSEL, banksel);
	DPRINTF(("winbond chip id 0x%x\n", chipid));
	switch(chipid) {
	case WB_CHIPID_W83627HF:
		printf(": W83627HF\n");
		lm_setup_sensors(sc, w83627hf_sensors);
		break;
	case WB_CHIPID_W83627THF:
		printf(": W83627THF\n");
		lm_setup_sensors(sc, w83627hf_sensors);
		break;
	case WB_CHIPID_W83637HF:
		printf(": W83637HF\n");
		lm_setup_sensors(sc, w83637hf_sensors);
		break;
	case WB_CHIPID_W83697HF:
		printf(": W83697HF\n");
		lm_setup_sensors(sc, w83697hf_sensors);
		break;
	case WB_CHIPID_W83781D:
	case WB_CHIPID_W83781D_2:
		printf(": W83781D\n");
		lm_setup_sensors(sc, w83781d_sensors);
		break;
	case WB_CHIPID_W83782D:
		printf(": W83782D\n");
		lm_setup_sensors(sc, w83782d_sensors);
		break;
	case WB_CHIPID_W83783S:
		printf(": W83783S\n");
		lm_setup_sensors(sc, w83783s_sensors);
		break;
	case WB_CHIPID_W83791D:
	case WB_CHIPID_W83791D_2:
		printf(": W83791D\n");
		lm_setup_sensors(sc, w83791d_sensors);
		break;
	case WB_CHIPID_AS99127F:
		if (vendid == WB_VENDID_ASUS) {
			printf(": AS99127F\n");
			lm_setup_sensors(sc, w83781d_sensors);
		} else {
			printf(": AS99127F rev 2\n");
			lm_setup_sensors(sc, as99127f_sensors);
		}
		break;
	default:
		printf(": unknown Winbond chip (ID 0x%x)\n", chipid);
		/* Handle as a standard LM78. */
		lm_setup_sensors(sc, lm78_sensors);
		sc->refresh_sensor_data = lm_refresh_sensor_data;
		return 1;
	}

	sc->refresh_sensor_data = wb_refresh_sensor_data;
	return 1;
}

void
lm_setup_sensors(struct lm_softc *sc, struct lm_sensor *sensors)
{
	int i;

	for (i = 0; sensors[i].desc; i++) {
		strlcpy(sc->sensors[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sensors[i].device));
		sc->sensors[i].type = sensors[i].type;
		strlcpy(sc->sensors[i].desc, sensors[i].desc,
		     sizeof(sc->sensors[i].desc));
		sc->sensors[i].rfact = sensors[i].rfact;
		sc->numsensors++;
	}
	sc->lm_sensors = sensors;
}

void
lm_refresh(void *arg)
{
	struct lm_softc *sc = arg;

	sc->refresh_sensor_data(sc);
}

void
lm_refresh_sensor_data(struct lm_softc *sc)
{
	int i;

	for (i = 0; i < sc->numsensors; i++)
		sc->lm_sensors[i].refresh(sc, i);
}

void
lm_refresh_volts(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int data;

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	sensor->value = (data << 4);
	sensor->value *= sensor->rfact;
	sensor->value /= 10;
}

void
lm_refresh_nvolts(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int data;

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	sensor->value = (data << 4);
	sensor->value *= sensor->rfact;
	sensor->value /= 10;
	sensor->value *= -1;
}

void
lm_refresh_temp(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int sdata;

	sdata = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	if (sdata & 0x80)
		sdata -= 0x100;
	sensor->value = sdata * 1000000 + 273150000;
}

void
lm_refresh_fanrpm(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int data, divisor = 1;

	/*
	 * We might get more accurate fan readings by adjusting the
	 * divisor, but that might interfere with APM or other SMM
	 * BIOS code reading the fan speeds.
	 */

	/* FAN3 has a fixed fan divisor. */
	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2) {
		data = sc->lm_readreg(sc, LMD_VIDFAN);
		if (sc->lm_sensors[n].reg == LMD_FAN1)
			divisor = (data >> 4) & 0x03;
		else
			divisor = (data >> 6) & 0x03;
	}

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	if (data == 0xff || data == 0x00) {
		sensor->flags |= SENSOR_FINVALID;
		sensor->value = 0;
	} else {
		sensor->flags &= ~SENSOR_FINVALID;
		sensor->value = 1350000 / (data << divisor);
	}
}

void
wb_refresh_sensor_data(struct lm_softc *sc)
{
	int banksel, bank, i;

	/*
	 * Properly save and restore bank selection register.
	 */

	banksel = bank = sc->lm_readreg(sc, WB_BANKSEL);
	for (i = 0; i < sc->numsensors; i++) {
		if (bank != sc->lm_sensors[i].bank) {
			bank = sc->lm_sensors[i].bank;
			sc->lm_writereg(sc, WB_BANKSEL, bank);
		}
		sc->lm_sensors[i].refresh(sc, i);
	}
	sc->lm_writereg(sc, WB_BANKSEL, banksel);
}

void
wb_refresh_n12volts(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int data;

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	sensor->value = (((data << 4) * 1000) - (WB_VREF * 806)) / 194;
	sensor->value *= sensor->rfact;
	sensor->value /= 10;
}

void
wb_refresh_n5volts(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int data;

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	sensor->value = (((data << 4) * 1000) - (WB_VREF * 682)) / 318;
	sensor->value *= sensor->rfact;
	sensor->value /= 10;
}

void
wb_refresh_temp(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int sdata;

	sdata = sc->lm_readreg(sc, sc->lm_sensors[n].reg) << 1;
	sdata += sc->lm_readreg(sc, sc->lm_sensors[n].reg + 1) >> 7;
	if (sdata & 0x100)
		sdata -= 0x200;
	sensor->value = sdata * 500000 + 273150000;
}

void
wb_refresh_fanrpm(struct lm_softc *sc, int n)
{
	struct sensor *sensor = &sc->sensors[n];
	int fan, data, divisor = 0;

	/* 
	 * This is madness; the fan divisor bits are scattered all
	 * over the place.
	 */

	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2 ||
	    sc->lm_sensors[n].reg == LMD_FAN3) {
		data = sc->lm_readreg(sc, WB_BANK0_FANBAT);
		fan = (sc->lm_sensors[n].reg - LMD_FAN1);
		if ((data >> 5) & (1 << fan))
			divisor |= 0x04;
	}

	if (sc->lm_sensors[n].reg == LMD_FAN1 ||
	    sc->lm_sensors[n].reg == LMD_FAN2) {
		data = sc->lm_readreg(sc, LMD_VIDFAN);
		if (sc->lm_sensors[n].reg == LMD_FAN1)
			divisor |= (data >> 4) & 0x03;
		else
			divisor |= (data >> 6) & 0x03;
	} else if (sc->lm_sensors[n].reg == LMD_FAN3) {
		data = sc->lm_readreg(sc, WB_PIN);
		divisor |= (data >> 6) & 0x03;
	} else if (sc->lm_sensors[n].reg == WB_BANK0_FAN4 ||
		   sc->lm_sensors[n].reg == WB_BANK0_FAN5) {
		data = sc->lm_readreg(sc, WB_BANK0_FAN45);
		if (sc->lm_sensors[n].reg == WB_BANK0_FAN4)
			divisor |= (data >> 0) & 0x07;
		else
			divisor |= (data >> 4) & 0x07;
	}

	data = sc->lm_readreg(sc, sc->lm_sensors[n].reg);
	if (data == 0xff || data == 0x00) {
		sensor->flags |= SENSOR_FINVALID;
		sensor->value = 0;
	} else {
		sensor->flags &= ~SENSOR_FINVALID;
		sensor->value = 1350000 / (data << divisor);
	}
}
