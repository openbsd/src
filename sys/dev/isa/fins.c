/*	$OpenBSD: fins.c,v 1.1 2008/03/19 19:33:09 deraadt Exp $	*/

/*
 * Copyright (c) 2005, 2006 Mark Kettenis
 * Copyright (c) 2007, 2008 Geoff Steckel
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
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sensors.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/* Derived from LM78 code.  Only handles chips attached to ISA bus */

/*
 * Fintek F71805 registers and constants
 * http://www.fintek.com.tw/files/productfiles/F71805F_V025.pdf
 * This chip is a multi-io chip with many functions.
 * Each function may be relocated in I/O space by the BIOS.
 * The base address (2E or 4E) accesses a configuration space which
 * has pointers to the individual functions. The config space must be
 * unlocked with a cookie and relocked afterwards. The chip ID is stored
 * in config space so it is not normally visible.
 *
 * We assume that the monitor is enabled. We don't try to start or stop it.
 * The voltage dividers specified are from reading the chips on one board.
 * There is no way to determine what they are in the general case.
 * This section of the chip controls the fans. We don't do anything to them.
 */


#define FINS_UNLOCK	0x87	/* magic constant - write 2x to select chip */
#define FINS_LOCK	0xaa	/* magic constant - write 1x to deselect reg */

#define FINS_FUNC_SEL	0x07	/* select which subchip to access */
#  define FINS_FUNC_SENSORS 0x4

/* ISA registers index to an internal register space on chip */
#define FINS_ADDR	0x00
#define FINS_DATA	0x01

/* Chip identification regs and values in bank 0 */
#define FINS_MANUF	0x23	/* manufacturer ID */
# define FINTEK_ID	0x1934
#define FINS_CHIP	0x20	/* chip ID */
# define FINS_ID	0x0406

/* in bank sensors of config space */
#define FINS_SENSADDR	0x60	/* sensors assigned I/O address (2 bytes) */

/* in sensors space */
#define FINS_TMODE	0x01	/* temperature mode reg */

#define FINS_MAX_SENSORS	20
/*
 * Fintek chips typically measure voltages using 8mv steps.
 * To measure higher voltages the input is attenuated with (external)
 * resistors.  Negative voltages are measured using inverting op amps
 * and resistors.  So we have to convert the sensor values back to
 * real voltages by applying the appropriate resistor factor.
 */
#define FRFACT_NONE	8000
#define FRFACT(x, y)	(FRFACT_NONE * ((x) + (y)) / (y))
#define FNRFACT(x, y)	(-FRFACT_NONE * (x) / (y))

#if defined(FINSDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

struct fins_softc;

struct fins_sensor {
	char *fs_desc;
	enum sensor_type fs_type;
	u_int8_t fs_aux;
	u_int8_t fs_reg;
	void (*fs_refresh)(struct fins_softc *, int);
	int fs_rfact;
};

struct fins_softc {
	struct device sc_dev;

	struct ksensor fins_ksensors[FINS_MAX_SENSORS];
	struct ksensordev fins_sensordev;
	struct sensor_task *fins_sensortask;
	struct fins_sensor *fins_sensors;
	u_int fins_numsensors;
	void (*refresh_sensor_data) (struct fins_softc *);

	u_int8_t (*fins_readreg)(struct fins_softc *, int);
	void (*fins_writereg)(struct fins_softc *, int, int);
	u_int fins_tempsel;
};


struct fins_isa_softc {
	struct fins_softc sc_finssc;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

int  fins_isa_match(struct device *, void *, void *);
void fins_isa_attach(struct device *, struct device *, void *);
u_int8_t fins_isa_readreg(struct fins_softc *, int);
void fins_isa_writereg(struct fins_softc *, int, int);

void fins_setup_sensors(struct fins_softc *, struct fins_sensor *);
void fins_refresh(void *);

void fins_refresh_volt(struct fins_softc *, int);
void fins_refresh_temp(struct fins_softc *, int);
void fins_refresh_offset(struct fins_softc *, int);
void fins_refresh_fanrpm(struct fins_softc *, int);
void fins_attach(struct fins_softc *);
int fins_detach(struct fins_softc *);

struct cfattach fins_ca = {
	sizeof(struct fins_isa_softc),
	fins_isa_match,
	fins_isa_attach
};

struct cfdriver fins_cd = {
	NULL, "fins", DV_DULL
};

struct fins_sensor fins_sensors[] = {
	/* Voltage */
	{ "+3.3V", SENSOR_VOLTS_DC, 0, 0x10, fins_refresh_volt, FRFACT(100, 100) },
	{ "Vtt", SENSOR_VOLTS_DC, 0, 0x11, fins_refresh_volt, FRFACT_NONE },
	{ "Vram", SENSOR_VOLTS_DC, 0, 0x12, fins_refresh_volt, FRFACT(100, 100) },
	{ "Vchips", SENSOR_VOLTS_DC, 0, 0x13, fins_refresh_volt, FRFACT(47, 100) },
	{ "+5V", SENSOR_VOLTS_DC, 0, 0x14, fins_refresh_volt, FRFACT(200, 47) },
	{ "+12V", SENSOR_VOLTS_DC, 0, 0x15, fins_refresh_volt, FRFACT(200, 20) },
	{ "Vcc 1.5V", SENSOR_VOLTS_DC, 0, 0x16, fins_refresh_volt, FRFACT_NONE },
	{ "VCore", SENSOR_VOLTS_DC, 0, 0x17, fins_refresh_volt, FRFACT_NONE },
	{ "Vsb", SENSOR_VOLTS_DC, 0, 0x18, fins_refresh_volt, FRFACT(200, 47) },
	{ "Vsbint", SENSOR_VOLTS_DC, 0, 0x19, fins_refresh_volt, FRFACT(200, 47) },
	{ "Vbat", SENSOR_VOLTS_DC, 0, 0x1A, fins_refresh_volt, FRFACT(200, 47) },

	/* Temperature */
	{ "Temp1", SENSOR_TEMP, 0x01, 0x1B, fins_refresh_temp },
	{ "Temp2", SENSOR_TEMP, 0x02, 0x1C, fins_refresh_temp },
	{ "Temp3", SENSOR_TEMP, 0x04, 0x1D, fins_refresh_temp },

	/* Fans */
	{ "Fan1", SENSOR_FANRPM, 0, 0x20, fins_refresh_fanrpm },
	{ "Fan2", SENSOR_FANRPM, 0, 0x22, fins_refresh_fanrpm },
	{ "Fan3", SENSOR_FANRPM, 0, 0x24, fins_refresh_fanrpm },

	{ NULL }
};


int
fins_isa_match(struct device *parent, void *match, void *aux)
{
	bus_space_tag_t iot;
	bus_addr_t iobase;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	u_int val;

	iot = ia->ia_iot;
	iobase = ia->ipa_io[0].base;

	if (bus_space_map(iot, iobase, 2, 0, &ioh))
		return (0);

	/* Fintek uses magic cookie locks to distinguish their chips */
	bus_space_write_1(iot, ioh, 0, FINS_UNLOCK);
	bus_space_write_1(iot, ioh, 0, FINS_UNLOCK);
	bus_space_write_1(iot, ioh, 0, FINS_FUNC_SEL);
	bus_space_write_1(iot, ioh, 1, 0);	/* IDs appear only in space 0 */
	bus_space_write_1(iot, ioh, 0, FINS_MANUF);
	val = bus_space_read_1(iot, ioh, 1) << 8;
	bus_space_write_1(iot, ioh, 0, FINS_MANUF + 1);
	val |= bus_space_read_1(iot, ioh, 1);
	if (val != FINTEK_ID) {
		bus_space_write_1(iot, ioh, 0, FINS_LOCK);
		goto notfound;
	}
	bus_space_write_1(iot, ioh, 0, FINS_CHIP);
	val = bus_space_read_1(iot, ioh, 1) << 8;
	bus_space_write_1(iot, ioh, 0, FINS_CHIP + 1);
	val |= bus_space_read_1(iot, ioh, 1);
	/* If we cared which Fintek chip this was we would save the chip ID here */
	if (val != FINS_ID) {
		bus_space_write_1(iot, ioh, 0, FINS_LOCK);
		goto notfound;
	}
	/* select sensor function of the chip */
	bus_space_write_1(iot, ioh, FINS_ADDR, FINS_FUNC_SEL);
	bus_space_write_1(iot, ioh, FINS_DATA, FINS_FUNC_SENSORS);
	/* read I/O address assigned by BIOS to this function */
	bus_space_write_1(iot, ioh, FINS_ADDR, FINS_SENSADDR);
	val = bus_space_read_1(iot, ioh, FINS_DATA) << 8;
	bus_space_write_1(iot, ioh, FINS_ADDR, FINS_SENSADDR + 1);
	val |= bus_space_read_1(iot, ioh, FINS_DATA);
	bus_space_write_1(iot, ioh, 0, FINS_LOCK);
	ia->ipa_io[1].length = 2;
	ia->ipa_io[1].base = val;

	bus_space_unmap(iot, ioh, 2);
	ia->ipa_nio = 2;
	ia->ipa_io[0].length = 2;
	ia->ipa_nmem = 0;
	ia->ipa_nirq = 0;
	ia->ipa_ndrq = 0;
	return (1);

 notfound:
	bus_space_unmap(iot, ioh, 2);
	return (0);
}

void
fins_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct fins_isa_softc *sc = (struct fins_isa_softc *)self;
	struct isa_attach_args *ia = aux;
	bus_addr_t iobase;

	sc->sc_iot = ia->ia_iot;
	iobase = ia->ipa_io[1].base;
	sc->sc_finssc.fins_writereg = fins_isa_writereg;
	sc->sc_finssc.fins_readreg = fins_isa_readreg;
	if (bus_space_map(sc->sc_iot, iobase, 2, 0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}
	fins_attach(&sc->sc_finssc);
}

u_int8_t
fins_isa_readreg(struct fins_softc *lmsc, int reg)
{
	struct fins_isa_softc *sc = (struct fins_isa_softc *)lmsc;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, FINS_ADDR, reg);
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh, FINS_DATA));
}

void
fins_isa_writereg(struct fins_softc *lmsc, int reg, int val)
{
	struct fins_isa_softc *sc = (struct fins_isa_softc *)lmsc;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, FINS_ADDR, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, FINS_DATA, val);
}

void
fins_attach(struct fins_softc *sc)
{
	u_int i;

	fins_setup_sensors(sc, fins_sensors);
	sc->fins_sensortask = sensor_task_register(sc, fins_refresh, 5);
	if (sc->fins_sensortask == NULL) {
		printf(": unable to register update task\n");
		return;
	}

	printf("\n");
	/* Add sensors */
	for (i = 0; i < sc->fins_numsensors; ++i)
		sensor_attach(&sc->fins_sensordev, &sc->fins_ksensors[i]);
	sensordev_install(&sc->fins_sensordev);
}

int
fins_detach(struct fins_softc *sc)
{
	int i;

	/* Remove sensors */
	sensordev_deinstall(&sc->fins_sensordev);
	for (i = 0; i < sc->fins_numsensors; i++)
		sensor_detach(&sc->fins_sensordev, &sc->fins_ksensors[i]);

	if (sc->fins_sensortask != NULL)
		sensor_task_unregister(sc->fins_sensortask);

	return 0;
}


void
fins_setup_sensors(struct fins_softc *sc, struct fins_sensor *sensors)
{
	int i;
	struct ksensor *ksensor = sc->fins_ksensors;

	strlcpy(sc->fins_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->fins_sensordev.xname));

	for (i = 0; sensors[i].fs_desc; i++) {
		ksensor[i].type = sensors[i].fs_type;
		strlcpy(ksensor[i].desc, sensors[i].fs_desc,
			sizeof(ksensor[i].desc));
	}
	sc->fins_numsensors = i;
	sc->fins_sensors = sensors;
	sc->fins_tempsel = sc->fins_readreg(sc, FINS_TMODE);
}

void
fins_refresh(void *arg)
{
	struct fins_softc *sc = arg;
	int i;

	for (i = 0; i < sc->fins_numsensors; i++)
		sc->fins_sensors[i].fs_refresh(sc, i);
}

void
fins_refresh_volt(struct fins_softc *sc, int n)
{
	struct ksensor *sensor = &sc->fins_ksensors[n];
	struct fins_sensor *fs = &sc->fins_sensors[n];
	int data;

	data = sc->fins_readreg(sc, fs->fs_reg);
	if (data == 0xff || data == 0) {
		sensor->flags |= SENSOR_FINVALID;
		sensor->value = 0;
	} else {
		sensor->flags &= ~SENSOR_FINVALID;
		sensor->value = data * fs->fs_rfact;
	}
}

/* The BIOS seems to add a fudge factor to the CPU temp of +5C */
void
fins_refresh_temp(struct fins_softc *sc, int n)
{
	struct ksensor *sensor = &sc->fins_ksensors[n];
	struct fins_sensor *fs = &sc->fins_sensors[n];
	u_int data;
	u_int max;

	/*
	 * The data sheet says that the range of the temperature
	 * sensor is between 0 and 127 or 140 degrees C depending on
	 * what kind of sensor is used.
	 * A disconnected sensor seems to read over 110 or so.
	 */
	data = sc->fins_readreg(sc, fs->fs_reg) & 0xFF;
	max = (sc->fins_tempsel & fs->fs_aux) ? 111 : 128;
	if (data == 0 || data >= max) {	/* disconnected? */
		sensor->flags |= SENSOR_FINVALID;
		sensor->value = 0;
	} else {
		sensor->flags &= ~SENSOR_FINVALID;
		sensor->value = data * 1000000 + 273150000;
	}
}

/* The chip holds a fudge factor for BJT sensors */
/* this is currently unused but might be reenabled */
void
fins_refresh_offset(struct fins_softc *sc, int n)
{
	struct ksensor *sensor = &sc->fins_ksensors[n];
	struct fins_sensor *fs = &sc->fins_sensors[n];
	u_int data;

	sensor->flags &= ~SENSOR_FINVALID;
	data = sc->fins_readreg(sc, fs->fs_reg);
	data |= ~0 * (data & 0x40);	/* sign extend 7-bit value */
	sensor->value = data * 1000000 + 273150000;
}


/* fan speed appears to be a 12-bit number */
void
fins_refresh_fanrpm(struct fins_softc *sc, int n)
{
	struct ksensor *sensor = &sc->fins_ksensors[n];
	struct fins_sensor *fs = &sc->fins_sensors[n];
	int data;

	data = sc->fins_readreg(sc, fs->fs_reg) << 8;
	data |= sc->fins_readreg(sc, fs->fs_reg + 1);
	if (data >= 0xfff) {
		sensor->value = 0;
		sensor->flags |= SENSOR_FINVALID;
	} else {
		sensor->value = 1500000 / data;
		sensor->flags &= ~SENSOR_FINVALID;
	}
}
