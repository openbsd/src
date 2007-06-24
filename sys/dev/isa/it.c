/*	$OpenBSD: it.c,v 1.23 2007/06/24 05:34:35 dlg Exp $	*/

/*
 * Copyright (c) 2003 Julien Bordet <zejames@greyhats.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sensors.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/itvar.h>

#if defined(ITDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * IT87-compatible chips can typically measure voltages up to 4.096 V.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using a reference
 * voltage.  So we have to convert the sensor values back to real
 * voltages by applying the appropriate resistor factor.
 */
#define RFACT_NONE	10000
#define RFACT(x, y)	(RFACT_NONE * ((x) + (y)) / (y))

int  it_match(struct device *, void *, void *);
void it_attach(struct device *, struct device *, void *);
u_int8_t it_readreg(struct it_softc *, int);
void it_writereg(struct it_softc *, int, int);
void it_setup_volt(struct it_softc *, int, int);
void it_setup_temp(struct it_softc *, int, int);
void it_setup_fan(struct it_softc *, int, int);

void it_generic_stemp(struct it_softc *, struct ksensor *);
void it_generic_svolt(struct it_softc *, struct ksensor *);
void it_generic_fanrpm(struct it_softc *, struct ksensor *);

void it_refresh_sensor_data(struct it_softc *);
void it_refresh(void *);

struct cfattach it_ca = {
	sizeof(struct it_softc),
	it_match,
	it_attach
};

struct cfdriver it_cd = {
	NULL, "it", DV_DULL
};

const int it_vrfact[] = {
	RFACT_NONE,
	RFACT_NONE,
	RFACT_NONE,
	RFACT(68, 100),
	RFACT(30, 10),
	RFACT(21, 10),
	RFACT(83, 20),
	RFACT(68, 100),
	RFACT_NONE
};

int
it_match(struct device *parent, void *match, void *aux)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int iobase;
	u_int8_t cr;

	iot = ia->ia_iot;
	iobase = ia->ipa_io[0].base;

	if (bus_space_map(iot, iobase, 8, 0, &ioh)) {
		DPRINTF(("it: can't map i/o space\n"));
		return (0);
	}

	/* Check Vendor ID */
	bus_space_write_1(iot, ioh, ITC_ADDR, ITD_CHIPID);
	cr = bus_space_read_1(iot, ioh, ITC_DATA);
	bus_space_unmap(iot, ioh, 8);
	DPRINTF(("it: vendor id 0x%x\n", cr));
	if (cr != IT_ID_IT87)
		return (0);

	ia->ipa_nio = 1;
	ia->ipa_io[0].length = 8;
	ia->ipa_nmem = 0;
	ia->ipa_nirq = 0;
	ia->ipa_ndrq = 0;

	return (1);
}

void
it_attach(struct device *parent, struct device *self, void *aux)
{
	struct it_softc *sc = (void *)self;
	int iobase;
	bus_space_tag_t iot;
	struct isa_attach_args *ia = aux;
	int i;
	u_int8_t cr;

	iobase = ia->ipa_io[0].base;
	iot = sc->it_iot = ia->ia_iot;

	if (bus_space_map(iot, iobase, 8, 0, &sc->it_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	i = it_readreg(sc, ITD_CHIPID);
	switch (i) {
		case IT_ID_IT87:
			printf(": IT87\n");
			break;
	}

	sc->numsensors = IT_NUM_SENSORS;

	it_setup_fan(sc, 0, 3);
	it_setup_volt(sc, 3, 9);
	it_setup_temp(sc, 12, 3);

	if (sensor_task_register(sc, it_refresh, 5) == NULL) {
		printf("%s: unable to register update task\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Activate monitoring */
	cr = it_readreg(sc, ITD_CONFIG);
	cr |= 0x01 | 0x08;
	it_writereg(sc, ITD_CONFIG, cr);

	/* Initialize sensors */
	strlcpy(sc->sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sensordev.xname));
	for (i = 0; i < sc->numsensors; ++i)
		sensor_attach(&sc->sensordev, &sc->sensors[i]);
	sensordev_install(&sc->sensordev);
}

u_int8_t
it_readreg(struct it_softc *sc, int reg)
{
	bus_space_write_1(sc->it_iot, sc->it_ioh, ITC_ADDR, reg);
	return (bus_space_read_1(sc->it_iot, sc->it_ioh, ITC_DATA));
}

void
it_writereg(struct it_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->it_iot, sc->it_ioh, ITC_ADDR, reg);
	bus_space_write_1(sc->it_iot, sc->it_ioh, ITC_DATA, val);
}

void
it_setup_volt(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		sc->sensors[start + i].type = SENSOR_VOLTS_DC;
	}

	snprintf(sc->sensors[start + 0].desc, sizeof(sc->sensors[0].desc),
	    "VCORE_A");
	snprintf(sc->sensors[start + 1].desc, sizeof(sc->sensors[1].desc),
	    "VCORE_B");
	snprintf(sc->sensors[start + 2].desc, sizeof(sc->sensors[2].desc),
	    "+3.3V");
	snprintf(sc->sensors[start + 3].desc, sizeof(sc->sensors[3].desc),
	    "+5V");
	snprintf(sc->sensors[start + 4].desc, sizeof(sc->sensors[4].desc),
	    "+12V");
	snprintf(sc->sensors[start + 5].desc, sizeof(sc->sensors[5].desc),
	    "Unused");
	snprintf(sc->sensors[start + 6].desc, sizeof(sc->sensors[6].desc),
	    "-12V");
	snprintf(sc->sensors[start + 7].desc, sizeof(sc->sensors[7].desc),
	    "+5VSB");
	snprintf(sc->sensors[start + 8].desc, sizeof(sc->sensors[8].desc),
	    "VBAT");
}

void
it_setup_temp(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i)
		sc->sensors[start + i].type = SENSOR_TEMP;
}

void
it_setup_fan(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i)
		sc->sensors[start + i].type = SENSOR_FANRPM;
}

void
it_generic_stemp(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 3; i++) {
		sdata = it_readreg(sc, ITD_SENSORTEMPBASE + i);
		/* Convert temperature to Fahrenheit degres */
		sensors[i].value = sdata * 1000000 + 273150000;
	}
}

void
it_generic_svolt(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 9; i++) {
		sdata = it_readreg(sc, ITD_SENSORVOLTBASE + i);
		DPRINTF(("sdata[volt%d] 0x%x\n", i, sdata));
		/* voltage returned as (mV >> 4) */
		sensors[i].value = (sdata << 4);
		/* these two values are negative and formula is different */
		if (i == 5 || i == 6)
			sensors[i].value = ((sdata << 4) - IT_VREF);
		/* rfact is (factor * 10^4) */
		sensors[i].value *= it_vrfact[i];
		/* division by 10 gets us back to uVDC */
		sensors[i].value /= 10;
		if (i == 5 || i == 6)
			sensors[i].value += IT_VREF * 1000;
	}
}

void
it_generic_fanrpm(struct it_softc *sc, struct ksensor *sensors)
{
	int i, sdata, divisor, odivisor, ndivisor;

	odivisor = ndivisor = divisor = it_readreg(sc, ITD_FAN);
	for (i = 0; i < 3; i++, divisor >>= 3) {
		sensors[i].flags &= ~SENSOR_FINVALID;
		if ((sdata = it_readreg(sc, ITD_SENSORFANBASE + i)) == 0xff) {
			sensors[i].flags |= SENSOR_FINVALID;
			if (i == 2)
				ndivisor ^= 0x40;
			else {
				ndivisor &= ~(7 << (i * 3));
				ndivisor |= ((divisor + 1) & 7) << (i * 3);
			}
		} else if (sdata == 0) {
			sensors[i].value = 0;
		} else {
			if (i == 2)
				divisor = divisor & 1 ? 3 : 1;
			sensors[i].value = 1350000 / (sdata << (divisor & 7));
		}
	}
	if (ndivisor != odivisor)
		it_writereg(sc, ITD_FAN, ndivisor);
}

/*
 * pre:  last read occurred >= 1.5 seconds ago
 * post: sensors[] current data are the latest from the chip
 */
void
it_refresh_sensor_data(struct it_softc *sc)
{
	/* Refresh our stored data for every sensor */
	it_generic_stemp(sc, &sc->sensors[12]);
	it_generic_svolt(sc, &sc->sensors[3]);
	it_generic_fanrpm(sc, &sc->sensors[0]);
}

void
it_refresh(void *arg)
{
	struct it_softc *sc = (struct it_softc *)arg;

	it_refresh_sensor_data(sc);
}
