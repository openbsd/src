/*	$OpenBSD: nslm7x.c,v 1.12 2005/12/30 19:47:39 kettenis Exp $	*/
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

void setup_fan(struct lm_softc *, int, int);
void setup_temp(struct lm_softc *, int, int);
void wb_setup_volt(struct lm_softc *);

int  lm_match(struct lm_softc *);
int  wb_match(struct lm_softc *);
int  def_match(struct lm_softc *);
void lm_common_match(struct lm_softc *);
int  lm_generic_banksel(struct lm_softc *, int);

void generic_stemp(struct lm_softc *, struct sensor *);
void generic_svolt(struct lm_softc *, struct sensor *);
void generic_fanrpm(struct lm_softc *, struct sensor *);

void lm_refresh_sensor_data(struct lm_softc *);

void wb_svolt(struct lm_softc *);
void wb_stemp(struct lm_softc *, struct sensor *, int);
void wb781_fanrpm(struct lm_softc *, struct sensor *);
void wb_fanrpm(struct lm_softc *, struct sensor *, int);

void wb781_refresh_sensor_data(struct lm_softc *);
void wb782_refresh_sensor_data(struct lm_softc *);
void wb697_refresh_sensor_data(struct lm_softc *);

void lm_refresh(void *);

#if 0
int lm_gtredata(struct sysmon_envsys *, struct envsys_tre_data *);

int generic_streinfo_fan(struct lm_softc *, struct envsys_basic_info *,
	int, struct envsys_basic_info *);
int lm_streinfo(struct sysmon_envsys *, struct envsys_basic_info *);
int wb781_streinfo(struct sysmon_envsys *, struct envsys_basic_info *);
int wb782_streinfo(struct sysmon_envsys *, struct envsys_basic_info *);
#endif

struct lm_chip {
	int (*chip_match)(struct lm_softc *);
};

struct lm_chip lm_chips[] = {
	{ wb_match },
	{ lm_match },
	{ def_match } /* Must be last */
};

int
lm_generic_banksel(struct lm_softc *lmsc, int bank)
{
	(*lmsc->lm_writereg)(lmsc, WB_BANKSEL, bank);
	return (0);
}

/*
 * bus independent probe
 */
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

/*
 * pre:  lmsc contains valid busspace tag and handle
 */
void
lm_attach(struct lm_softc *lmsc)
{
	u_int i;

	/* Install default bank selection routine, if none given. */
	if (lmsc->lm_banksel == NULL)
		lmsc->lm_banksel = lm_generic_banksel;

	for (i = 0; i < sizeof(lm_chips) / sizeof(lm_chips[0]); i++)
		if (lm_chips[i].chip_match(lmsc))
			break;

	if (sensor_task_register(lmsc, lm_refresh, 5)) {
		printf("%s: unable to register update task\n",
		    lmsc->sc_dev.dv_xname);
		return;
	}

	/* Start the monitoring loop */
	(*lmsc->lm_writereg)(lmsc, LMD_CONFIG, 0x01);

	/* Initialize sensors */
	for (i = 0; i < lmsc->numsensors; ++i) {
		strlcpy(lmsc->sensors[i].device, lmsc->sc_dev.dv_xname,
		    sizeof(lmsc->sensors[i].device));
		SENSOR_ADD(&lmsc->sensors[i]);
	}
}

int
lm_match(struct lm_softc *sc)
{
	int i;

	/* See if we have an LM78 or LM79 */
	i = (*sc->lm_readreg)(sc, LMD_CHIPID) & LM_ID_MASK;
	switch(i) {
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
	lm_common_match(sc);
	return 1;
}

int
def_match(struct lm_softc *sc)
{
	int i;

	i = (*sc->lm_readreg)(sc, LMD_CHIPID) & LM_ID_MASK;
	printf(": unknown chip (ID %d)\n", i);
	lm_common_match(sc);
	return 1;
}

void
lm_common_match(struct lm_softc *sc)
{
	int i;
	sc->numsensors = LM_NUM_SENSORS;
	sc->refresh_sensor_data = lm_refresh_sensor_data;

	for (i = 0; i < 7; ++i) {
		sc->sensors[i].type = SENSOR_VOLTS_DC;
		snprintf(sc->sensors[i].desc, sizeof(sc->sensors[i].desc),
		    "IN%d", i);
	}

	/* default correction factors for resistors on higher voltage inputs */
	sc->sensors[0].rfact = sc->sensors[1].rfact =
	    sc->sensors[2].rfact = 10000;
	sc->sensors[3].rfact = (int)(( 90.9 / 60.4) * 10000);
	sc->sensors[4].rfact = (int)(( 38.0 / 10.0) * 10000);
	sc->sensors[5].rfact = (int)((210.0 / 60.4) * 10000);
	sc->sensors[6].rfact = (int)(( 90.9 / 60.4) * 10000);

	sc->sensors[7].type = SENSOR_TEMP;
	strlcpy(sc->sensors[7].desc, "Temp", sizeof(sc->sensors[7].desc));

	setup_fan(sc, 8, 3);
}

int
wb_match(struct lm_softc *sc)
{
	int i, j;

	(*sc->lm_writereg)(sc, WB_BANKSEL, WB_BANKSEL_HBAC);
	j = (*sc->lm_readreg)(sc, WB_VENDID) << 8;
	(*sc->lm_writereg)(sc, WB_BANKSEL, 0);
	j |= (*sc->lm_readreg)(sc, WB_VENDID);
	DPRINTF(("winbond vend id 0x%x\n", j));
	if (j != WB_VENDID_WINBOND && j != WB_VENDID_ASUS)
		return 0;
	/* read device ID */
	(*sc->lm_banksel)(sc, 0);
	j = (*sc->lm_readreg)(sc, WB_BANK0_CHIPID);
	DPRINTF(("winbond chip id 0x%x\n", j));
	switch(j) {
	case WB_CHIPID_83781:
	case WB_CHIPID_83781_2:
	case AS_CHIPID_99127:
		if (j == AS_CHIPID_99127)
			printf(": AS99127F\n");
		else
			printf(": W83781D\n");

		for (i = 0; i < 7; ++i) {
			sc->sensors[i].type = SENSOR_VOLTS_DC;
			snprintf(sc->sensors[i].desc,
			    sizeof(sc->sensors[i].desc), "IN%d", i);
		}

		/* default correction factors for higher voltage inputs */
		sc->sensors[0].rfact = sc->sensors[1].rfact =
		    sc->sensors[2].rfact = 10000;
		sc->sensors[3].rfact = (int)(( 90.9 / 60.4) * 10000);
		sc->sensors[4].rfact = (int)(( 38.0 / 10.0) * 10000);
		sc->sensors[5].rfact = (int)((210.0 / 60.4) * 10000);
		sc->sensors[6].rfact = (int)(( 90.9 / 60.4) * 10000);

		setup_temp(sc, 7, 3);
		setup_fan(sc, 10, 3);

		sc->numsensors = WB83781_NUM_SENSORS;
		sc->refresh_sensor_data = wb781_refresh_sensor_data;
		return 1;
	case WB_CHIPID_83697:
		printf(": W83697HF\n");
		wb_setup_volt(sc);
		setup_temp(sc, 9, 2);
		setup_fan(sc, 11, 2);
		sc->numsensors = WB83697_NUM_SENSORS;
		sc->refresh_sensor_data = wb697_refresh_sensor_data;
		return 1;
	case WB_CHIPID_83782:
		printf(": W83782D\n");
		break;
	case WB_CHIPID_83627:
		printf(": W83627HF\n");
		break;
	case WB_CHIPID_83627THF:
		printf(": W83627THF\n");
		break;
	case WB_CHIPID_83791:
	case WB_CHIPID_83791_2:
		printf(": W83791D\n");
		break;
	default:
		printf(": unknown winbond chip ID 0x%x\n", j);
		/* handle as a standart lm7x */
		lm_common_match(sc);
		return 1;
	}
	/* common code for the W83782D and W83627HF */
	wb_setup_volt(sc);
	setup_temp(sc, 9, 3);
	setup_fan(sc, 12, 3);
	sc->numsensors = WB_NUM_SENSORS;
	sc->refresh_sensor_data = wb782_refresh_sensor_data;
	return 1;
}

void
wb_setup_volt(struct lm_softc *sc)
{
	sc->sensors[0].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[0].desc, sizeof(sc->sensors[0].desc), "VCORE_A");
	sc->sensors[0].rfact = 10000;
	sc->sensors[1].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[1].desc, sizeof(sc->sensors[1].desc), "VCORE_B");
	sc->sensors[1].rfact = 10000;
	sc->sensors[2].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[2].desc, sizeof(sc->sensors[2].desc), "+3.3V");
	sc->sensors[2].rfact = 10000;
	sc->sensors[3].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[3].desc, sizeof(sc->sensors[3].desc), "+5V");
	sc->sensors[3].rfact = 16778;
	sc->sensors[4].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[4].desc, sizeof(sc->sensors[4].desc), "+12V");
	sc->sensors[4].rfact = 38000;
	sc->sensors[5].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[5].desc, sizeof(sc->sensors[5].desc), "-12V");
	sc->sensors[5].rfact = 10000;
	sc->sensors[6].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[6].desc, sizeof(sc->sensors[6].desc), "-5V");
	sc->sensors[6].rfact = 10000;
	sc->sensors[7].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[7].desc, sizeof(sc->sensors[7].desc), "+5VSB");
	sc->sensors[7].rfact = 15151;
	sc->sensors[8].type = SENSOR_VOLTS_DC;
	snprintf(sc->sensors[8].desc, sizeof(sc->sensors[8].desc), "VBAT");
	sc->sensors[8].rfact = 10000;
}

void
setup_temp(struct lm_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		sc->sensors[start + i].type = SENSOR_TEMP;
		snprintf(sc->sensors[start + i].desc,
		    sizeof(sc->sensors[start + i].desc), "Temp%d", i + 1);
	}
}

void
setup_fan(struct lm_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		sc->sensors[start + i].type = SENSOR_FANRPM;
		snprintf(sc->sensors[start + i].desc,
		    sizeof(sc->sensors[start + i].desc), "Fan%d", i + 1);
	}
}

#if 0
int
lm_gtredata(sme, tred)
	 struct sysmon_envsys *sme;
	 struct envsys_tre_data *tred;
{
	 static const struct timeval onepointfive = { 1, 500000 };
	 struct timeval t, mtv;
	 struct lm_softc *sc = sme->sme_cookie;
	 int i;

	 /* read new values at most once every 1.5 seconds */
	 timeradd(&sc->lastread, &onepointfive, &t);
	 getmicrouptime(&mtv);
	 i = timercmp(&mtv, &t, >);
	 if (i) {
		  sc->lastread = mtv;
		  sc->refresh_sensor_data(sc);
	 }

	 *tred = sc->sensors[tred->sensor];

	 return (0);
}

int
generic_streinfo_fan(sc, info, n, binfo)
	struct lm_softc *sc;
	struct envsys_basic_info *info;
	int n;
	struct envsys_basic_info *binfo;
{
	u_int8_t sdata;
	int divisor;

	/* FAN1 and FAN2 can have divisors set, but not FAN3 */
	if ((sc->sensors[binfo->sensor].type == SENSOR_FANRPM)
	    && (n < 2)) {
		if (binfo->rpms == 0) {
			binfo->validflags = 0;
			return (0);
		}

		/* write back the nominal FAN speed  */
		info->rpms = binfo->rpms;

		/* 153 is the nominal FAN speed value */
		divisor = 1350000 / (binfo->rpms * 153);

		/* ...but we need lg(divisor) */
		if (divisor <= 1)
		    divisor = 0;
		else if (divisor <= 2)
		    divisor = 1;
		else if (divisor <= 4)
		    divisor = 2;
		else
		    divisor = 3;

		/*
		 * FAN1 div is in bits <5:4>, FAN2 div is
		 * in <7:6>
		 */
		sdata = (*sc->lm_readreg)(sc, LMD_VIDFAN);
		if ( n == 0 ) {  /* FAN1 */
		    divisor <<= 4;
		    sdata = (sdata & 0xCF) | divisor;
		} else { /* FAN2 */
		    divisor <<= 6;
		    sdata = (sdata & 0x3F) | divisor;
		}

		(*sc->lm_writereg)(sc, LMD_VIDFAN, sdata);
	}
	return (0);

}

int
lm_streinfo(sme, binfo)
	 struct sysmon_envsys *sme;
	 struct envsys_basic_info *binfo;
{
	 struct lm_softc *sc = sme->sme_cookie;

	 if (sc->sensors[binfo->sensor].type == SENSOR_VOLTS_DC)
		  sc->sensors[binfo->sensor].rfact = binfo->rfact;
	 else {
		if (sc->sensors[binfo->sensor].type == SENSOR_FANRPM) {
			generic_streinfo_fan(sc, &sc->sensors[binfo->sensor],
			    binfo->sensor - 8, binfo);
		}
		memcpy(sc->sensors[binfo->sensor].desc, binfo->desc,
		    sizeof(sc->sensors[binfo->sensor].desc));
		sc->sensors[binfo->sensor].desc[
		    sizeof(sc->sensors[binfo->sensor].desc) - 1] = '\0';

		binfo->validflags = ENVSYS_FVALID;
	 }
	 return (0);
}

int
wb781_streinfo(sme, binfo)
	 struct sysmon_envsys *sme;
	 struct envsys_basic_info *binfo;
{
	 struct lm_softc *sc = sme->sme_cookie;
	 int divisor;
	 u_int8_t sdata;
	 int i;

	 if (sc->sensors[binfo->sensor].type == SENSOR_VOLTS_DC)
		  sc->sensors[binfo->sensor].rfact = binfo->rfact;
	 else {
		if (sc->sensors[binfo->sensor].type == SENSOR_FANRPM) {
			if (binfo->rpms == 0) {
				binfo->validflags = 0;
				return (0);
			}

			/* write back the nominal FAN speed  */
			sc->sensors[binfo->sensor].rpms = binfo->rpms;

			/* 153 is the nominal FAN speed value */
			divisor = 1350000 / (binfo->rpms * 153);

			/* ...but we need lg(divisor) */
			for (i = 0; i < 7; i++) {
				if (divisor <= (1 << i))
				 	break;
			}
			divisor = i;

			if (binfo->sensor == 10 || binfo->sensor == 11) {
				/*
				 * FAN1 div is in bits <5:4>, FAN2 div
				 * is in <7:6>
				 */
				sdata = (*sc->lm_readreg)(sc, LMD_VIDFAN);
				if ( binfo->sensor == 10 ) {  /* FAN1 */
					 sdata = (sdata & 0xCF) |
					     ((divisor & 0x3) << 4);
				} else { /* FAN2 */
					 sdata = (sdata & 0x3F) |
					     ((divisor & 0x3) << 6);
				}
				(*sc->lm_writereg)(sc, LMD_VIDFAN, sdata);
			} else {
				/* FAN3 is in WB_PIN <7:6> */
				sdata = (*sc->lm_readreg)(sc, WB_PIN);
				sdata = (sdata & 0x3F) |
				     ((divisor & 0x3) << 6);
				(*sc->lm_writereg)(sc, WB_PIN, sdata);
			}
		}
		memcpy(sc->sensors[binfo->sensor].desc, binfo->desc,
		    sizeof(sc->sensors[binfo->sensor].desc));
		sc->sensors[binfo->sensor].desc[
		    sizeof(sc->sensors[binfo->sensor].desc) - 1] = '\0';

		binfo->validflags = ENVSYS_FVALID;
	 }
	 return (0);
}

int
wb782_streinfo(sme, binfo)
	 struct sysmon_envsys *sme;
	 struct envsys_basic_info *binfo;
{
	 struct lm_softc *sc = sme->sme_cookie;
	 int divisor;
	 u_int8_t sdata;
	 int i;

	 if (sc->sensors[binfo->sensor].type == SENSOR_VOLTS_DC)
		  sc->sensors[binfo->sensor].rfact = binfo->rfact;
	 else {
	 	if (sc->sensors[binfo->sensor].type == SENSOR_FANRPM) {
			if (binfo->rpms == 0) {
				binfo->validflags = 0;
				return (0);
			}

			/* write back the nominal FAN speed  */
			sc->sensors[binfo->sensor].rpms = binfo->rpms;

			/* 153 is the nominal FAN speed value */
			divisor = 1350000 / (binfo->rpms * 153);

			/* ...but we need lg(divisor) */
			for (i = 0; i < 7; i++) {
				if (divisor <= (1 << i))
				 	break;
			}
			divisor = i;

			if (binfo->sensor == 12 || binfo->sensor == 13) {
				/*
				 * FAN1 div is in bits <5:4>, FAN2 div
				 * is in <7:6>
				 */
				sdata = (*sc->lm_readreg)(sc, LMD_VIDFAN);
				if ( binfo->sensor == 12 ) {  /* FAN1 */
					 sdata = (sdata & 0xCF) |
					     ((divisor & 0x3) << 4);
				} else { /* FAN2 */
					 sdata = (sdata & 0x3F) |
					     ((divisor & 0x3) << 6);
				}
				(*sc->lm_writereg)(sc, LMD_VIDFAN, sdata);
			} else {
				/* FAN3 is in WB_PIN <7:6> */
				sdata = (*sc->lm_readreg)(sc, WB_PIN);
				sdata = (sdata & 0x3F) |
				     ((divisor & 0x3) << 6);
				(*sc->lm_writereg)(sc, WB_PIN, sdata);
			}
			/* Bit 2 of divisor is in WB_BANK0_FANBAT */
			(*sc->lm_banksel)(sc, 0);
			sdata = (*sc->lm_readreg)(sc, WB_BANK0_FANBAT);
			sdata &= ~(0x20 << (binfo->sensor - 12));
			sdata |= (divisor & 0x4) << (binfo->sensor - 9);
			(*sc->lm_writereg)(sc, WB_BANK0_FANBAT, sdata);
		}

		memcpy(sc->sensors[binfo->sensor].desc, binfo->desc,
		    sizeof(sc->sensors[binfo->sensor].desc));
		sc->sensors[binfo->sensor].desc[
		    sizeof(sc->sensors[binfo->sensor].desc) - 1] = '\0';

		binfo->validflags = ENVSYS_FVALID;
	}
	return (0);
}
#endif

void
generic_stemp(struct lm_softc *sc, struct sensor *sensor)
{
	int sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + 7);

	DPRINTF(("sdata[temp] 0x%x\n", sdata));
	/* temp is given in deg. C, we convert to uK */
	sensor->value = sdata * 1000000 + 273150000;
}

void
generic_svolt(struct lm_softc *sc, struct sensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 7; i++) {
		sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + i);
		DPRINTF(("sdata[volt%d] 0x%x\n", i, sdata));
		/* voltage returned as (mV >> 4), we convert to uVDC */
		sensors[i].value = (sdata << 4);
		/* rfact is (factor * 10^4) */
		sensors[i].value *= sensors[i].rfact;
		/* division by 10 gets us back to uVDC */
		sensors[i].value /= 10;

		/* these two are negative voltages */
		if ( (i == 5) || (i == 6) )
			sensors[i].value *= -1;
	}
}

void
generic_fanrpm(struct lm_softc *sc, struct sensor *sensors)
{
	int i, sdata, divisor, vidfan;

	for (i = 0; i < 3; i++) {
		sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + 8 + i);
		vidfan = (*sc->lm_readreg)(sc, LMD_VIDFAN);
		DPRINTF(("sdata[fan%d] 0x%x", i, sdata));
		if (i == 2)
			divisor = 1;	/* Fixed divisor for FAN3 */
		else if (i == 1)	/* Bits 7 & 6 of VID/FAN  */
			divisor = (vidfan >> 6) & 0x3;
		else
			divisor = (vidfan >> 4) & 0x3;
		DPRINTF((", divisor %d\n", 2 << divisor));

		if (sdata == 0xff) {
			/* Fan can be too slow, try to adjust the divisor */
			if (i < 2 && divisor < 3) {
				divisor++;
				vidfan &= ~(0x3 << (i == 0 ? 4 : 6));
				vidfan |= (divisor & 0x3) << (i == 0 ? 4 : 6);
				(*sc->lm_writereg)(sc, LMD_VIDFAN, vidfan);
			}
			sensors[i].value = 0;
		} else if (sdata == 0x00) {
			sensors[i].flags |= SENSOR_FINVALID;
			sensors[i].value = 0;
		} else {
			sensors[i].flags &= ~SENSOR_FINVALID;
			sensors[i].value = 1350000 / (sdata << divisor);
		}
	}
}

/*
 * pre:  last read occurred >= 1.5 seconds ago
 * post: sensors[] current data are the latest from the chip
 */
void
lm_refresh_sensor_data(struct lm_softc *sc)
{
	/* Refresh our stored data for every sensor */
	generic_stemp(sc, &sc->sensors[7]);
	generic_svolt(sc, &sc->sensors[0]);
	generic_fanrpm(sc, &sc->sensors[8]);
}

void
wb_svolt(struct lm_softc *sc)
{
	int i, sdata;

	for (i = 0; i < 9; ++i) {
		if (i < 7) {
			sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + i);
		} else {
			/* from bank5 */
			(*sc->lm_banksel)(sc, 5);
			sdata = (*sc->lm_readreg)(sc, (i == 7) ?
			    WB_BANK5_5VSB : WB_BANK5_VBAT);
		}
		DPRINTF(("sdata[volt%d] 0x%x\n", i, sdata));
		/* voltage returned as (mV >> 4), we convert to uV */
		sdata =  sdata << 4;
		/* special case for negative voltages */
		if (i == 5) {
			/*
			 * -12Vdc, assume Winbond recommended values for
			 * resistors
			 */
			sdata = ((sdata * 1000) - (3600 * 806)) / 194;
		} else if (i == 6) {
			/*
			 * -5Vdc, assume Winbond recommended values for
			 * resistors
			 */
			sdata = ((sdata * 1000) - (3600 * 682)) / 318;
		}
		/* rfact is (factor * 10^4) */
		sc->sensors[i].value = sdata * (int64_t)sc->sensors[i].rfact;
		/* division by 10 gets us back to uVDC */
		sc->sensors[i].value /= 10;
	}
}

void
wb_stemp(struct lm_softc *sc, struct sensor *sensors, int n)
{
	int sdata;

	/* temperatures. Given in dC, we convert to uK */
	sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + 7);
	DPRINTF(("sdata[temp0] 0x%x\n", sdata));
	sensors[0].value = sdata * 1000000 + 273150000;
	/* from bank1 */
	if ((*sc->lm_banksel)(sc, 1)) {
		sensors[1].flags |= SENSOR_FINVALID;
	} else {
		sdata = (*sc->lm_readreg)(sc, WB_BANK1_T2H) << 1;
		sdata |=  ((*sc->lm_readreg)(sc, WB_BANK1_T2L) & 0x80) >> 7;
		DPRINTF(("sdata[temp1] 0x%x\n", sdata));
		sensors[1].value = (sdata * 1000000) / 2 + 273150000;
	}
	if (n < 3)
		return;
	/* from bank2 */
	if ((*sc->lm_banksel)(sc, 2)) {
		sensors[2].flags |= SENSOR_FINVALID;
	} else {
		sdata = (*sc->lm_readreg)(sc, WB_BANK2_T3H) << 1;
		sdata |=  ((*sc->lm_readreg)(sc, WB_BANK2_T3L) & 0x80) >> 7;
		DPRINTF(("sdata[temp2] 0x%x\n", sdata));
		sensors[2].value = (sdata * 1000000) / 2 + 273150000;
	}
}

void
wb781_fanrpm(struct lm_softc *sc, struct sensor *sensors)
{
	int i, divisor, sdata;

	(*sc->lm_banksel)(sc, 0);
	for (i = 0; i < 3; i++) {
		sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + i + 8);
		DPRINTF(("sdata[fan%d] 0x%x\n", i, sdata));
		if (i == 0)
			divisor = ((*sc->lm_readreg)(sc,
			    LMD_VIDFAN) >> 4) & 0x3;
		else if (i == 1)
			divisor = ((*sc->lm_readreg)(sc,
			    LMD_VIDFAN) >> 6) & 0x3;
		else
			divisor = ((*sc->lm_readreg)(sc, WB_PIN) >> 6) & 0x3;

		DPRINTF(("sdata[%d] 0x%x div 0x%x\n", i, sdata, divisor));
		if (sdata == 0xff) {
			sensors[i].flags |= SENSOR_FINVALID;
		} else if (sdata == 0x00) {
			sensors[i].value = 0;
		} else {
			sensors[i].value = 1350000 / (sdata << divisor);
		}
	}
}

void
wb_fanrpm(struct lm_softc *sc, struct sensor *sensors, int n)
{
	int i, divisor, sdata;

	(*sc->lm_banksel)(sc, 0);
	for (i = 0; i < n; i++) {
		sdata = (*sc->lm_readreg)(sc, LMD_SENSORBASE + i + 8);
		DPRINTF(("sdata[fan%d] 0x%x\n", i, sdata));
		if (i == 0)
			divisor = ((*sc->lm_readreg)(sc,
			    LMD_VIDFAN) >> 4) & 0x3;
		else if (i == 1)
			divisor = ((*sc->lm_readreg)(sc,
			    LMD_VIDFAN) >> 6) & 0x3;
		else
			divisor = ((*sc->lm_readreg)(sc, WB_PIN) >> 6) & 0x3;
		divisor |= ((*sc->lm_readreg)(sc,
		    WB_BANK0_FANBAT) >> (i + 3)) & 0x4;

		DPRINTF(("sdata[%d] 0x%x div 0x%x\n", i, sdata, divisor));
		if (sdata == 0xff) {
			sensors[i].flags |= SENSOR_FINVALID;
		} else if (sdata == 0x00) {
			sensors[i].value = 0;
		} else {
			sensors[i].value = 1350000 / (sdata << divisor);
		}
	}
}

void
wb781_refresh_sensor_data(struct lm_softc *sc)
{
	/* Refresh our stored data for every sensor */
	/* we need to reselect bank0 to access common registers */
	(*sc->lm_banksel)(sc, 0);
	generic_svolt(sc, &sc->sensors[0]);
	(*sc->lm_banksel)(sc, 0);
	wb_stemp(sc, &sc->sensors[7], 3);
	(*sc->lm_banksel)(sc, 0);
	wb781_fanrpm(sc, &sc->sensors[10]);
}

void
wb782_refresh_sensor_data(struct lm_softc *sc)
{
	/* Refresh our stored data for every sensor */
	wb_svolt(sc);
	wb_stemp(sc, &sc->sensors[9], 3);
	wb_fanrpm(sc, &sc->sensors[12], 3);
}

void
wb697_refresh_sensor_data(struct lm_softc *sc)
{
	/* Refresh our stored data for every sensor */
	wb_svolt(sc);
	wb_stemp(sc, &sc->sensors[9], 2);
	wb_fanrpm(sc, &sc->sensors[11], 2);
}

void
lm_refresh(void *arg)
{
	struct lm_softc *sc = (struct lm_softc *)arg;

	sc->refresh_sensor_data(sc);
}
