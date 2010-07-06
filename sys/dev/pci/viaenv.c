/*	$OpenBSD: viaenv.c,v 1.14 2010/07/06 09:05:41 blambert Exp $	*/
/*	$NetBSD: viaenv.c,v 1.9 2002/10/02 16:51:59 thorpej Exp $	*/

/*
 * Copyright (c) 2000 Johan Danielsson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of author nor the names of any contributors may
 *    be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
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

/* driver for the hardware monitoring part of the VIA VT82C686A */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sensors.h>
#include <sys/timeout.h>
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#ifdef VIAENV_DEBUG
unsigned int viaenv_debug = 0;
#define DPRINTF(X) do { if(viaenv_debug) printf X ; } while(0)
#else
#define DPRINTF(X)
#endif

#define VIANUMSENSORS 10	/* three temp, two fan, five voltage */

struct viaenv_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_pm_ioh;

	int     sc_fan_div[2];	/* fan RPM divisor */

	struct ksensor sc_data[VIANUMSENSORS];
	struct ksensordev sc_sensordev;
};

int  viaenv_match(struct device *, void *, void *);
void viaenv_attach(struct device *, struct device *, void *);

int  val_to_uK(unsigned int);
int  val_to_rpm(unsigned int, int);
long val_to_uV(unsigned int, int);
void viaenv_refresh_sensor_data(struct viaenv_softc *);
void viaenv_refresh(void *);

#ifdef __HAVE_TIMECOUNTER
u_int viaenv_get_timecount(struct timecounter *tc);

struct timecounter viaenv_timecounter = {
	viaenv_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffff,		/* counter_mask */
	3579545,		/* frequency */
	"VIAPM",		/* name */
	1000			/* quality */
};
#endif	/* __HAVE_TIMECOUNTER */

struct cfattach viaenv_ca = {
	sizeof(struct viaenv_softc),
	viaenv_match,
	viaenv_attach
};

struct cfdriver viaenv_cd = {
	NULL, "viaenv", DV_DULL
};

struct timeout viaenv_timeout;

const struct pci_matchid viaenv_devices[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT82C686A_SMB },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8231_PWR }
};

int
viaenv_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, viaenv_devices,
	    sizeof(viaenv_devices) / sizeof(viaenv_devices[0])));
}

/*
 * XXX there doesn't seem to exist much hard documentation on how to
 * convert the raw values to usable units, this code is more or less
 * stolen from the Linux driver, but changed to suit our conditions
 */

/*
 * lookup-table to translate raw values to uK, this is the same table
 * used by the Linux driver (modulo units); there is a fifth degree
 * polynomial that supposedly been used to generate this table, but I
 * haven't been able to figure out how -- it doesn't give the same values
 */

static const long val_to_temp[] = {
	20225, 20435, 20645, 20855, 21045, 21245, 21425, 21615, 21785, 21955,
	22125, 22285, 22445, 22605, 22755, 22895, 23035, 23175, 23315, 23445,
	23565, 23695, 23815, 23925, 24045, 24155, 24265, 24365, 24465, 24565,
	24665, 24765, 24855, 24945, 25025, 25115, 25195, 25275, 25355, 25435,
	25515, 25585, 25655, 25725, 25795, 25865, 25925, 25995, 26055, 26115,
	26175, 26235, 26295, 26355, 26405, 26465, 26515, 26575, 26625, 26675,
	26725, 26775, 26825, 26875, 26925, 26975, 27025, 27065, 27115, 27165,
	27205, 27255, 27295, 27345, 27385, 27435, 27475, 27515, 27565, 27605,
	27645, 27685, 27735, 27775, 27815, 27855, 27905, 27945, 27985, 28025,
	28065, 28105, 28155, 28195, 28235, 28275, 28315, 28355, 28405, 28445,
	28485, 28525, 28565, 28615, 28655, 28695, 28735, 28775, 28825, 28865,
	28905, 28945, 28995, 29035, 29075, 29125, 29165, 29205, 29245, 29295,
	29335, 29375, 29425, 29465, 29505, 29555, 29595, 29635, 29685, 29725,
	29765, 29815, 29855, 29905, 29945, 29985, 30035, 30075, 30125, 30165,
	30215, 30255, 30305, 30345, 30385, 30435, 30475, 30525, 30565, 30615,
	30655, 30705, 30755, 30795, 30845, 30885, 30935, 30975, 31025, 31075,
	31115, 31165, 31215, 31265, 31305, 31355, 31405, 31455, 31505, 31545,
	31595, 31645, 31695, 31745, 31805, 31855, 31905, 31955, 32005, 32065,
	32115, 32175, 32225, 32285, 32335, 32395, 32455, 32515, 32575, 32635,
	32695, 32755, 32825, 32885, 32955, 33025, 33095, 33155, 33235, 33305,
	33375, 33455, 33525, 33605, 33685, 33765, 33855, 33935, 34025, 34115,
	34205, 34295, 34395, 34495, 34595, 34695, 34805, 34905, 35015, 35135,
	35245, 35365, 35495, 35615, 35745, 35875, 36015, 36145, 36295, 36435,
	36585, 36745, 36895, 37065, 37225, 37395, 37575, 37755, 37935, 38125,
	38325, 38525, 38725, 38935, 39155, 39375, 39605, 39835, 40075, 40325,
	40575, 40835, 41095, 41375, 41655, 41935,
};

/* use above table to convert values to temperatures in micro-Kelvins */
int
val_to_uK(unsigned int val)
{
	int     i = val / 4;
	int     j = val % 4;

	KASSERT(i >= 0 && i <= 255);

	if (j == 0 || i == 255)
		return val_to_temp[i] * 10000;

	/* is linear interpolation ok? */
	return (val_to_temp[i] * (4 - j) +
	    val_to_temp[i + 1] * j) * 2500 /* really: / 4 * 10000 */ ;
}

int
val_to_rpm(unsigned int val, int div)
{

	if (val == 0)
		return 0;

	return 1350000 / val / div;
}

long
val_to_uV(unsigned int val, int index)
{
	static const long mult[] =
	    {1250000, 1250000, 1670000, 2600000, 6300000};

	KASSERT(index >= 0 && index <= 4);

	return (25LL * val + 133) * mult[index] / 2628;
}

#define VIAENV_TSENS3	0x1f
#define VIAENV_TSENS1	0x20
#define VIAENV_TSENS2	0x21
#define VIAENV_VSENS1	0x22
#define VIAENV_VSENS2	0x23
#define VIAENV_VCORE	0x24
#define VIAENV_VSENS3	0x25
#define VIAENV_VSENS4	0x26
#define VIAENV_FAN1	0x29
#define VIAENV_FAN2	0x2a
#define VIAENV_FANCONF	0x47	/* fan configuration */
#define VIAENV_TLOW	0x49	/* temperature low order value */
#define VIAENV_TIRQ	0x4b	/* temperature interrupt configuration */

#define VIAENV_GENCFG	0x40	/* general configuration */
#define VIAENV_GENCFG_TMR32	(1 << 11)	/* 32-bit PM timer */
#define VIAENV_GENCFG_PMEN	(1 << 15)	/* enable PM I/O space */
#define VIAENV_PMBASE	0x48	/* power management I/O space base */
#define VIAENV_PMSIZE	128	/* power management I/O space size */
#define VIAENV_PM_TMR	0x08	/* PM timer */

void
viaenv_refresh_sensor_data(struct viaenv_softc *sc)
{
	int i;
	u_int8_t v, v2;

	/* temperature */
	v = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_TIRQ);
	v2 = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_TSENS1);
	DPRINTF(("TSENS1 = %d\n", (v2 << 2) | (v >> 6)));
	sc->sc_data[0].value = val_to_uK((v2 << 2) | (v >> 6));

	v = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_TLOW);
	v2 = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_TSENS2);
	DPRINTF(("TSENS2 = %d\n", (v2 << 2) | ((v >> 4) & 0x3)));
	sc->sc_data[1].value = val_to_uK((v2 << 2) | ((v >> 4) & 0x3));

	v2 = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_TSENS3);
	DPRINTF(("TSENS3 = %d\n", (v2 << 2) | (v >> 6)));
	sc->sc_data[2].value = val_to_uK((v2 << 2) | (v >> 6));

	v = bus_space_read_1(sc->sc_iot, sc->sc_ioh, VIAENV_FANCONF);

	sc->sc_fan_div[0] = 1 << ((v >> 4) & 0x3);
	sc->sc_fan_div[1] = 1 << ((v >> 6) & 0x3);

	/* fan */
	for (i = 3; i <= 4; i++) {
		v = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    VIAENV_FAN1 + i - 3);
		DPRINTF(("FAN%d = %d / %d\n", i - 3, v,
		    sc->sc_fan_div[i - 3]));
		sc->sc_data[i].value = val_to_rpm(v, sc->sc_fan_div[i - 3]);
	}

	/* voltage */
	for (i = 5; i <= 9; i++) {
		v = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    VIAENV_VSENS1 + i - 5);
		DPRINTF(("V%d = %d\n", i - 5, v));
		sc->sc_data[i].value = val_to_uV(v, i - 5);
	}
}

void
viaenv_attach(struct device * parent, struct device * self, void *aux)
{
	struct viaenv_softc *sc = (struct viaenv_softc *) self;
	struct pci_attach_args *pa = aux;
	pcireg_t iobase, control;
	int i;

	iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x70);
	control = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x74);
	if ((iobase & 0xff80) == 0 || (control & 1) == 0) {
		printf(": HWM disabled");
		goto nohwm;
	}
	sc->sc_iot = pa->pa_iot;
	if (bus_space_map(sc->sc_iot, iobase & 0xff80, 128, 0, &sc->sc_ioh)) {
		printf(": can't map HWM i/o space");
		goto nohwm;
	}

	for (i = 0; i <= 2; i++) {
		sc->sc_data[i].type = SENSOR_TEMP;
	}

	for (i = 3; i <= 4; i++) {
		sc->sc_data[i].type = SENSOR_FANRPM;
	}

	for (i = 5; i <= 9; ++i) {
		sc->sc_data[i].type = SENSOR_VOLTS_DC;
	}
	strlcpy(sc->sc_data[5].desc, "VSENS1",
	    sizeof(sc->sc_data[5].desc));	/* CPU core (2V) */
	strlcpy(sc->sc_data[6].desc, "VSENS2",
	    sizeof(sc->sc_data[6].desc));	/* NB core? (2.5V) */
	strlcpy(sc->sc_data[7].desc, "Vcore",
	    sizeof(sc->sc_data[7].desc));	/* Vcore (3.3V) */
	strlcpy(sc->sc_data[8].desc, "VSENS3",
	    sizeof(sc->sc_data[8].desc));	/* VSENS3 (5V) */
	strlcpy(sc->sc_data[9].desc, "VSENS4",
	    sizeof(sc->sc_data[9].desc));	/* VSENS4 (12V) */

	/* Get initial set of sensor values. */
	viaenv_refresh_sensor_data(sc);

	/* Register sensors with sysctl */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	for (i = 0; i < VIANUMSENSORS; ++i)
		sensor_attach(&sc->sc_sensordev, &sc->sc_data[i]);
	sensordev_install(&sc->sc_sensordev);

	/* Refresh sensors data every 1.5 seconds */
	timeout_set(&viaenv_timeout, viaenv_refresh, sc);
	timeout_add_msec(&viaenv_timeout, 1500);

nohwm:
#ifdef __HAVE_TIMECOUNTER
	/* Check if power management I/O space is enabled */
	control = pci_conf_read(pa->pa_pc, pa->pa_tag, VIAENV_GENCFG);
	if ((control & VIAENV_GENCFG_PMEN) == 0) {
		printf(": PM disabled");
		goto nopm;
	}

	/* Map power management I/O space */
	iobase = pci_conf_read(pa->pa_pc, pa->pa_tag, VIAENV_PMBASE);
	if (bus_space_map(sc->sc_iot, PCI_MAPREG_IO_ADDR(iobase),
	    VIAENV_PMSIZE, 0, &sc->sc_pm_ioh)) {
		printf(": can't map PM i/o space");
		goto nopm;
	}

	/* Check for 32-bit PM timer */
	if (control & VIAENV_GENCFG_TMR32)
		viaenv_timecounter.tc_counter_mask = 0xffffffff;

	/* Register new timecounter */
	viaenv_timecounter.tc_priv = sc;
	tc_init(&viaenv_timecounter);

	printf(": %s-bit timer at %lluHz",
	    (viaenv_timecounter.tc_counter_mask == 0xffffffff ? "32" : "24"),
	    (unsigned long long)viaenv_timecounter.tc_frequency);

nopm:
#endif	/* __HAVE_TIMECOUNTER */
	printf("\n");
}

void
viaenv_refresh(void *arg)
{
	struct viaenv_softc *sc = (struct viaenv_softc *)arg;

	viaenv_refresh_sensor_data(sc);
	timeout_add_msec(&viaenv_timeout, 1500);
}

#ifdef __HAVE_TIMECOUNTER
u_int
viaenv_get_timecount(struct timecounter *tc)
{
	struct viaenv_softc *sc = tc->tc_priv;
	u_int u1, u2, u3;

	u2 = bus_space_read_4(sc->sc_iot, sc->sc_pm_ioh, VIAENV_PM_TMR);
	u3 = bus_space_read_4(sc->sc_iot, sc->sc_pm_ioh, VIAENV_PM_TMR);
	do {
		u1 = u2;
		u2 = u3;
		u3 = bus_space_read_4(sc->sc_iot, sc->sc_pm_ioh,
		    VIAENV_PM_TMR);
	} while (u1 > u2 || u2 > u3);

	return (u2);
}
#endif	/* __HAVE_TIMECOUNTER */
