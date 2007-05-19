/*	$OpenBSD: aps.c,v 1.15 2007/05/19 19:14:11 tedu Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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
 * A driver for the ThinkPad Active Protection System based on notes from
 * http://www.almaden.ibm.com/cs/people/marksmith/tpaps.html
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

#if defined(APSDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

#define APS_ACCEL_STATE		0x04
#define APS_INIT		0x10
#define APS_STATE		0x11
#define	APS_XACCEL		0x12
#define APS_YACCEL		0x14
#define APS_TEMP		0x16
#define	APS_XVAR		0x17
#define APS_YVAR		0x19
#define APS_TEMP2		0x1b
#define APS_UNKNOWN		0x1c
#define APS_INPUT		0x1d
#define APS_CMD			0x1f

#define	APS_STATE_NEWDATA	0x50

#define APS_CMD_START		0x01

#define APS_INPUT_KB		(1 << 5)
#define APS_INPUT_MS		(1 << 6)
#define APS_INPUT_LIDOPEN	(1 << 7)

#define APS_ADDR_SIZE		0x1f

struct sensor_rec {
	u_int8_t	state;
	u_int16_t	x_accel;
	u_int16_t	y_accel;
	u_int8_t	temp1;
	u_int16_t	x_var;
	u_int16_t	y_var;
	u_int8_t	temp2;
	u_int8_t	unk;
	u_int8_t	input;
};

#define APS_NUM_SENSORS		9

#define APS_SENSOR_XACCEL	0
#define APS_SENSOR_YACCEL	1
#define APS_SENSOR_XVAR		2
#define APS_SENSOR_YVAR		3
#define APS_SENSOR_TEMP1	4
#define APS_SENSOR_TEMP2	5
#define APS_SENSOR_KBACT	6
#define APS_SENSOR_MSACT	7
#define APS_SENSOR_LIDOPEN	8

struct aps_softc {
	struct device sc_dev;

	bus_space_tag_t aps_iot;
	bus_space_handle_t aps_ioh;

	struct ksensor sensors[APS_NUM_SENSORS];
	struct ksensordev sensordev;
	void (*refresh_sensor_data)(struct aps_softc *);

	struct sensor_rec aps_data;
};

int	 aps_match(struct device *, void *, void *);
void	 aps_attach(struct device *, struct device *, void *);

int	 aps_init(bus_space_tag_t, bus_space_handle_t);
u_int8_t aps_mem_read_1(bus_space_tag_t, bus_space_handle_t, int, u_int8_t);
int	 aps_read_data(struct aps_softc *);
void	 aps_refresh_sensor_data(struct aps_softc *sc);
void	 aps_refresh(void *);
void	 aps_power(int, void *);

struct cfattach aps_ca = {
	sizeof(struct aps_softc),
	aps_match,
	aps_attach
};

struct cfdriver aps_cd = {
	NULL, "aps", DV_DULL
};

struct timeout aps_timeout;

int
aps_match(struct device *parent, void *match, void *aux)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int iobase, i;
	u_int8_t cr;

	iot = ia->ia_iot;
	iobase = ia->ipa_io[0].base;

	if (bus_space_map(iot, iobase, APS_ADDR_SIZE, 0, &ioh)) {
		DPRINTF(("aps: can't map i/o space\n"));
		return (0);
	}

	/* See if this machine has APS */
	bus_space_write_1(iot, ioh, APS_INIT, 0x13);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);

	/* ask again as the X40 is slightly deaf in one ear */
	bus_space_read_1(iot, ioh, APS_CMD);
	bus_space_write_1(iot, ioh, APS_INIT, 0x13);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);

	if (!aps_mem_read_1(iot, ioh, APS_CMD, 0x00)) {
		bus_space_unmap(iot, ioh, APS_ADDR_SIZE);
		return (0);
	}

	/*
	 * Observed values from Linux driver:
	 * 0x01: T42
	 * 0x02: chip already initialised
	 * 0x03: T41
	 */
	for (i = 0; i < 10; i++) {
		cr = bus_space_read_1(iot, ioh, APS_STATE);
		if (cr > 0 && cr < 6)
			break;
		delay(5 * 1000);
	}
	
	bus_space_unmap(iot, ioh, APS_ADDR_SIZE);
	DPRINTF(("aps: state register 0x%x\n", cr));
	if (cr < 1 || cr > 5) {
		DPRINTF(("aps0: unsupported state %d\n", cr));
		return (0);
	}

	ia->ipa_nio = 1;
	ia->ipa_io[0].length = APS_ADDR_SIZE;
	ia->ipa_nmem = 0;
	ia->ipa_nirq = 0;
	ia->ipa_ndrq = 0;

	return (1);
}

void
aps_attach(struct device *parent, struct device *self, void *aux)
{
	struct aps_softc *sc = (void *)self;
	int iobase, i;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;

	iobase = ia->ipa_io[0].base;
	iot = sc->aps_iot = ia->ia_iot;

	if (bus_space_map(iot, iobase, APS_ADDR_SIZE, 0, &sc->aps_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	ioh = sc->aps_ioh;

	printf("\n");

	if (!aps_init(iot, ioh))
		goto out;

	sc->sensors[APS_SENSOR_XACCEL].type = SENSOR_INTEGER;
	snprintf(sc->sensors[APS_SENSOR_XACCEL].desc,
	    sizeof(sc->sensors[APS_SENSOR_XACCEL].desc), "X_ACCEL");

	sc->sensors[APS_SENSOR_YACCEL].type = SENSOR_INTEGER;
	snprintf(sc->sensors[APS_SENSOR_YACCEL].desc,
	    sizeof(sc->sensors[APS_SENSOR_YACCEL].desc), "Y_ACCEL");

	sc->sensors[APS_SENSOR_TEMP1].type = SENSOR_TEMP;
	sc->sensors[APS_SENSOR_TEMP2].type = SENSOR_TEMP;

	sc->sensors[APS_SENSOR_XVAR].type = SENSOR_INTEGER;
	snprintf(sc->sensors[APS_SENSOR_XVAR].desc,
	    sizeof(sc->sensors[APS_SENSOR_XVAR].desc), "X_VAR");

	sc->sensors[APS_SENSOR_YVAR].type = SENSOR_INTEGER;
	snprintf(sc->sensors[APS_SENSOR_YVAR].desc,
	    sizeof(sc->sensors[APS_SENSOR_YVAR].desc), "Y_VAR");

	sc->sensors[APS_SENSOR_KBACT].type = SENSOR_INDICATOR;
	snprintf(sc->sensors[APS_SENSOR_KBACT].desc,
	    sizeof(sc->sensors[APS_SENSOR_KBACT].desc), "Keyboard Active");

	sc->sensors[APS_SENSOR_MSACT].type = SENSOR_INDICATOR;
	snprintf(sc->sensors[APS_SENSOR_MSACT].desc,
	    sizeof(sc->sensors[APS_SENSOR_MSACT].desc), "Mouse Active");

	sc->sensors[APS_SENSOR_LIDOPEN].type = SENSOR_INDICATOR;
	snprintf(sc->sensors[APS_SENSOR_LIDOPEN].desc,
	    sizeof(sc->sensors[APS_SENSOR_LIDOPEN].desc), "Lid Open");

	/* stop hiding and report to the authorities */
	strlcpy(sc->sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sensordev.xname));
	for (i = 0; i < APS_NUM_SENSORS ; i++) {
		sensor_attach(&sc->sensordev, &sc->sensors[i]);
	}
	sensordev_install(&sc->sensordev);

	powerhook_establish(aps_power, (void *)sc);

	/* Refresh sensor data every 0.5 seconds */
	timeout_set(&aps_timeout, aps_refresh, sc);
	timeout_add(&aps_timeout, (5 * hz) / 10);
	return;
out:
	printf("%s: failed to initialise\n", sc->sc_dev.dv_xname);
	return;
}

int
aps_init(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	bus_space_write_1(iot, ioh, APS_INIT, 0x17);
	bus_space_write_1(iot, ioh, APS_STATE, 0x81);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	if (!aps_mem_read_1(iot, ioh, APS_CMD, 0x00))
		return (0);
	if (!aps_mem_read_1(iot, ioh, APS_STATE, 0x00))
		return (0);
	if (!aps_mem_read_1(iot, ioh, APS_XACCEL, 0x60))
		return (0);
	if (!aps_mem_read_1(iot, ioh, APS_XACCEL + 1, 0x00))
		return (0);
	bus_space_write_1(iot, ioh, APS_INIT, 0x14);
	bus_space_write_1(iot, ioh, APS_STATE, 0x01);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	if (!aps_mem_read_1(iot, ioh, APS_CMD, 0x00))
		return (0);
	bus_space_write_1(iot, ioh, APS_INIT, 0x10);
	bus_space_write_1(iot, ioh, APS_STATE, 0xc8);
	bus_space_write_1(iot, ioh, APS_XACCEL, 0x00);
	bus_space_write_1(iot, ioh, APS_XACCEL + 1, 0x02);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	if (!aps_mem_read_1(iot, ioh, APS_CMD, 0x00))
		return (0);
	/* refresh data */
	bus_space_write_1(iot, ioh, APS_INIT, 0x11);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	if (!aps_mem_read_1(iot, ioh, APS_ACCEL_STATE, 0x50))
		return (0);
	if (!aps_mem_read_1(iot, ioh, APS_STATE, 0x00))
		return (0);

	return (1);
}

u_int8_t
aps_mem_read_1(bus_space_tag_t iot, bus_space_handle_t ioh, int reg,
    u_int8_t val)
{
	int i;
	u_int8_t cr;
	/* should take no longer than 50 microseconds */
	for (i = 0; i < 10; i++) {
		cr = bus_space_read_1(iot, ioh, reg);
		if (cr == val)
			return (1);
		delay(5 * 1000);
	}
	DPRINTF(("aps: reg 0x%x not val 0x%x!\n", reg, val));
	return (0);
}

int
aps_read_data(struct aps_softc *sc)
{
	bus_space_tag_t iot = sc->aps_iot;
	bus_space_handle_t ioh = sc->aps_ioh;

	sc->aps_data.state = bus_space_read_1(iot, ioh, APS_STATE);
	sc->aps_data.x_accel = bus_space_read_2(iot, ioh, APS_XACCEL);
	sc->aps_data.y_accel = bus_space_read_2(iot, ioh, APS_YACCEL);
	sc->aps_data.temp1 = bus_space_read_1(iot, ioh, APS_TEMP);
	sc->aps_data.x_var = bus_space_read_2(iot, ioh, APS_XVAR);
	sc->aps_data.y_var = bus_space_read_2(iot, ioh, APS_YVAR);
	sc->aps_data.temp2 = bus_space_read_1(iot, ioh, APS_TEMP2);
	sc->aps_data.input = bus_space_read_1(iot, ioh, APS_INPUT);

	return (1);
}

void
aps_refresh_sensor_data(struct aps_softc *sc)
{
	bus_space_tag_t iot = sc->aps_iot;
	bus_space_handle_t ioh = sc->aps_ioh;
	int64_t temp;
	int i;

	/* ask for new data */
	bus_space_write_1(iot, ioh, APS_INIT, 0x11);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	if (!aps_mem_read_1(iot, ioh, APS_ACCEL_STATE, 0x50))
		return;
	aps_read_data(sc);
	bus_space_write_1(iot, ioh, APS_INIT, 0x11);
	bus_space_write_1(iot, ioh, APS_CMD, 0x01);

	/* tell accelerometer we're done reading from it */
	bus_space_read_1(iot, ioh, APS_CMD);
	bus_space_read_1(iot, ioh, APS_ACCEL_STATE);

	for (i = 0; i < APS_NUM_SENSORS; i++) {
		sc->sensors[i].flags &= ~SENSOR_FINVALID;
	}

	sc->sensors[APS_SENSOR_XACCEL].value = sc->aps_data.x_accel;
	sc->sensors[APS_SENSOR_YACCEL].value = sc->aps_data.y_accel;

	/* convert to micro (mu) degrees */
	temp = sc->aps_data.temp1 * 1000000;	
	/* convert to kelvin */
	temp += 273150000; 
	sc->sensors[APS_SENSOR_TEMP1].value = temp;

	/* convert to micro (mu) degrees */
	temp = sc->aps_data.temp2 * 1000000;	
	/* convert to kelvin */
	temp += 273150000; 
	sc->sensors[APS_SENSOR_TEMP2].value = temp;

	sc->sensors[APS_SENSOR_XVAR].value = sc->aps_data.x_var;
	sc->sensors[APS_SENSOR_YVAR].value = sc->aps_data.y_var;
	sc->sensors[APS_SENSOR_KBACT].value =
	    (sc->aps_data.input &  APS_INPUT_KB) ? 1 : 0;
	sc->sensors[APS_SENSOR_MSACT].value =
	    (sc->aps_data.input & APS_INPUT_MS) ? 1 : 0;
	sc->sensors[APS_SENSOR_LIDOPEN].value =
	    (sc->aps_data.input & APS_INPUT_LIDOPEN) ? 1 : 0;
}

void
aps_refresh(void *arg)
{
	struct aps_softc *sc = (struct aps_softc *)arg;

	aps_refresh_sensor_data(sc);
	timeout_add(&aps_timeout, (5 * hz) / 10);
}

void
aps_power(int why, void *arg)
{
	struct aps_softc *sc = (struct aps_softc *)arg;
	bus_space_tag_t iot = sc->aps_iot;
	bus_space_handle_t ioh = sc->aps_ioh;

	if (why != PWR_RESUME) {
		if (timeout_pending(&aps_timeout))
			timeout_del(&aps_timeout);
	} else {
		/*
		 * Redo the init sequence on resume, because APS is 
		 * as forgetful as it is deaf.
		 */
		bus_space_write_1(iot, ioh, APS_INIT, 0x13);
		bus_space_write_1(iot, ioh, APS_CMD, 0x01);
		bus_space_read_1(iot, ioh, APS_CMD);
		bus_space_write_1(iot, ioh, APS_INIT, 0x13);
		bus_space_write_1(iot, ioh, APS_CMD, 0x01);
	
		if (aps_mem_read_1(iot, ioh, APS_CMD, 0x00) &&
		    aps_init(iot, ioh))
			timeout_add(&aps_timeout, (5 * hz) / 10);
		else
			printf("aps: failed to wake up\n");
	}
}

