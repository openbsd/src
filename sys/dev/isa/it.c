/*	$OpenBSD: it.c,v 1.10 2004/08/23 18:06:02 millert Exp $	*/

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
#include <sys/timeout.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/itvar.h>

#if defined(ITDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

int  it_match(struct device *, void *, void *);
void it_attach(struct device *, struct device *, void *);
u_int8_t it_readreg(struct it_softc *, int);
void it_writereg(struct it_softc *, int, int);
void it_setup_volt(struct it_softc *, int, int);
void it_setup_temp(struct it_softc *, int, int);
void it_setup_fan(struct it_softc *, int, int);

void it_generic_stemp(struct it_softc *, struct sensor *);
void it_generic_svolt(struct it_softc *, struct sensor *);
void it_generic_fanrpm(struct it_softc *, struct sensor *);

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

struct timeout it_timeout;

int
it_match(struct device *parent, void *match, void *aux)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int iobase;
	int rv;
	u_int8_t cr;

	iot = ia->ia_iot;
	iobase = ia->ipa_io[0].base;

	if (bus_space_map(iot, iobase, 8, 0, &ioh)) {
		DPRINTF(("%s: can't map i/o space\n", __func__));
		return (0);
	}

	/* Check for some power-on defaults */
	bus_space_write_1(iot, ioh, ITC_ADDR, ITD_CONFIG);
	cr = bus_space_read_1(iot, ioh, ITC_DATA);

	/* The monitoring may have been enabled by BIOS */
	if (cr == 0x11 || cr == 0x13 || cr == 0x18 || cr == 0x19)
		rv = 1;
	else
		rv = 0;

	DPRINTF(("it: rv = %d, cr = %x\n", rv, cr));

	bus_space_unmap(iot, ioh, 8);

	if (rv) {
		ia->ipa_nio = 1;
		ia->ipa_io[0].length = 8;

		ia->ipa_nmem = 0;
		ia->ipa_nirq = 0;
		ia->ipa_ndrq = 0;
	}

	return (rv);
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
		default:
			printf(": unknown chip (ID %d)\n", i);
			break;
	}

	sc->numsensors = IT_NUM_SENSORS;

	/* Reset chip */
	it_writereg(sc, ITD_CONFIG, 0x80);

	it_setup_fan(sc, 0, 3);
	it_setup_volt(sc, 3, 9);
	it_setup_temp(sc, 12, 3);

	/* Activate monitoring */
	cr = it_readreg(sc, ITD_CONFIG);
	cr |= 0x01 | 0x08;
	it_writereg(sc, ITD_CONFIG, cr);

	/* Initialize sensors */
	for (i = 0; i < sc->numsensors; ++i) {
		strlcpy(sc->sensors[i].device, sc->sc_dev.dv_xname,
		    sizeof(sc->sensors[i].device));
		SENSOR_ADD(&sc->sensors[i]);
	}

	timeout_set(&it_timeout, it_refresh, sc);
	timeout_add(&it_timeout, (15 * hz) / 10);
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

	sc->sensors[start + 0].rfact = 10000;
	snprintf(sc->sensors[start + 0].desc, sizeof(sc->sensors[0].desc), 
	    "VCORE_A");
	sc->sensors[start + 1].rfact = 10000;
	snprintf(sc->sensors[start + 1].desc, sizeof(sc->sensors[1].desc), 
	    "VCORE_B");
	sc->sensors[start + 2].rfact = 10000;
	snprintf(sc->sensors[start + 2].desc, sizeof(sc->sensors[2].desc), 
	    "+3.3V");
	sc->sensors[start + 3].rfact = (int)(( 16.8 / 10) * 10000);
	snprintf(sc->sensors[start + 3].desc, sizeof(sc->sensors[3].desc), 
	    "+5V");
	sc->sensors[start + 4].rfact = (int)(( 40 / 10) * 10000);
	snprintf(sc->sensors[start + 4].desc, sizeof(sc->sensors[4].desc), 
	    "+12V");
	sc->sensors[start + 5].rfact = (int)(( 31.0 / 10) * 10000);
	snprintf(sc->sensors[start + 5].desc, sizeof(sc->sensors[5].desc), 
	    "Unused");
	sc->sensors[start + 6].rfact = (int)(( 103.0 / 20) * 10000);
	snprintf(sc->sensors[start + 6].desc, sizeof(sc->sensors[6].desc), 
	    "-12V");
	sc->sensors[start + 7].rfact = (int)(( 16.8 / 10) * 10000);
	snprintf(sc->sensors[start + 7].desc, sizeof(sc->sensors[7].desc), 
	    "+5VSB");
	sc->sensors[start + 8].rfact = 10000;
	snprintf(sc->sensors[start + 8].desc, sizeof(sc->sensors[8].desc), 
	    "VBAT");

	/* Enable voltage monitoring */
	it_writereg(sc, ITD_VOLTENABLE, 0xff);
}

void
it_setup_temp(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		sc->sensors[start + i].type = SENSOR_TEMP;
		snprintf(sc->sensors[start + i].desc, 
		    sizeof(sc->sensors[start + i].desc),
		    "Temp%d", i + 1);
	}

	/* Enable temperature monitoring 
	 * bits 7 and 8 are reserved, so we don't change them */
	i = it_readreg(sc, ITD_TEMPENABLE) & 0xc0;
	it_writereg(sc, ITD_TEMPENABLE, i | 0x38);
}

void
it_setup_fan(struct it_softc *sc, int start, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		sc->sensors[start + i].type = SENSOR_FANRPM;
		snprintf(sc->sensors[start + i].desc, 
		    sizeof(sc->sensors[start + i].desc),
		    "Fan%d", i + 1);
	}

	/* Enable fan rpm monitoring
	 * bits 4 to 6 are the only interesting bits */
	i = it_readreg(sc, ITD_FANENABLE) & 0x8f;
	it_writereg(sc, ITD_FANENABLE, i | 0x70);
}

void
it_generic_stemp(struct it_softc *sc, struct sensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 3; i++) {
		sdata = it_readreg(sc, ITD_SENSORTEMPBASE + i);
		/* Convert temperature to Fahrenheit degres */
		sensors[i].value = sdata * 1000000 + 273150000;
	}
}

void
it_generic_svolt(struct it_softc *sc, struct sensor *sensors)
{
	int i, sdata;

	for (i = 0; i < 9; i++) {
		sdata = it_readreg(sc, ITD_SENSORVOLTBASE + i);
		DPRINTF(("sdata[volt%d] 0x%x\n", i, sdata));
		/* voltage returned as (mV >> 4) */
		sensors[i].value = (sdata << 4);
		/* rfact is (factor * 10^4) */
		sensors[i].value *= sensors[i].rfact;
		/* these two values are negative and formula is different */
		if (i == 5) 
			sensors[i].value -= 
			    (int) (21.0 / 10 * IT_VREF * 10000);
		if (i == 6) 
			sensors[i].value -= 
			    (int) (83.0 / 20 * IT_VREF * 10000);
		/* division by 10 gets us back to uVDC */
		sensors[i].value /= 10;

	}
}

void
it_generic_fanrpm(struct it_softc *sc, struct sensor *sensors)
{
	int i, sdata, divisor;

	for (i = 0; i < 3; i++) {
		sdata = it_readreg(sc, ITD_SENSORFANBASE + i);
		switch (i) {
			case 2:
				divisor = 2;
				break;
			case 1:
				divisor = (it_readreg(sc, ITD_FAN) >> 3) & 0x7;
				break;
			default:
				divisor = it_readreg(sc, ITD_FAN) & 0x7;
				break;
		}

		if (sdata == 0xff) {
			sensors[i].flags |= SENSOR_FINVALID;
		} else if (sdata == 0) {
			sensors[i].value = 0;
		} else {
			sensors[i].value = 1350000 / (sdata << divisor);
		}
	}
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
	timeout_add(&it_timeout, (15 * hz) / 10);
}
