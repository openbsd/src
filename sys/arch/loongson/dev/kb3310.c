/*	$OpenBSD: kb3310.c,v 1.4 2010/02/24 18:29:39 otto Exp $	*/
/*
 * Copyright (c) 2010 Otto Moerbeek <otto@drijf.net>
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/bus.h>
#include <dev/isa/isavar.h>

struct cfdriver ykbec_cd = {
	NULL, "ykbec", DV_DULL,
};

#define IO_YKBEC		0x381
#define IO_YKBECSIZE		0x3

#define KB3310_NUM_SENSORS	12

struct ykbec_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct ksensor		sc_sensor[KB3310_NUM_SENSORS];
	struct ksensordev	sc_sensordev;

};

int	ykbec_match(struct device *, void *, void *);
void	ykbec_attach(struct device *, struct device *, void *);
void	ykbec_refresh(void *arg);

const struct cfattach ykbec_ca = {
	sizeof(struct ykbec_softc), ykbec_match, ykbec_attach
};

void	ykbec_write(struct ykbec_softc *, u_int, u_int);
u_int	ykbec_read(struct ykbec_softc *, u_int);
u_int	ykbec_read16(struct ykbec_softc *, u_int);

int
ykbec_match(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;

	if ((ia->ia_iobase != IOBASEUNK && ia->ia_iobase != IO_YKBEC) ||
	    /* (ia->ia_iosize != 0 && ia->ia_iosize != IO_YKBECSIZE) || XXX isa.c */
	    ia->ia_maddr != MADDRUNK || ia->ia_msize != 0 ||
	    ia->ia_irq != IRQUNK || ia->ia_drq != DRQUNK)
		return (0);

	if (bus_space_map(ia->ia_iot, IO_YKBEC, IO_YKBECSIZE, 0, &ioh))
		return (0);

	bus_space_unmap(ia->ia_iot, ioh, IO_YKBECSIZE);

	ia->ia_iobase = IO_YKBEC;
	ia->ia_iosize = IO_YKBECSIZE;

	return (1);
}


void
ykbec_attach( struct device *parent, struct device *self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct ykbec_softc *sc = (struct ykbec_softc *)self;

	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, ia->ia_iosize, 0,
	    &sc->sc_ioh)) {
		printf(": couldn't map I/O space");
		return;
	}

	/* Initialize sensor data. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	if (sensor_task_register(sc, ykbec_refresh, 5) == NULL) {
		printf(", unable to register update task\n");
		return;
	}
	sc->sc_sensor[0].type = SENSOR_FANRPM;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[0]);

	sc->sc_sensor[1].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[1].desc, "Internal temperature",
	    sizeof(sc->sc_sensor[1].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[1]);

	sc->sc_sensor[2].type = SENSOR_AMPHOUR;
	strlcpy(sc->sc_sensor[2].desc, "Battery design capacity",
	    sizeof(sc->sc_sensor[2].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[2]);

	sc->sc_sensor[3].type = SENSOR_AMPHOUR;
	strlcpy(sc->sc_sensor[3].desc, "Battery full charge capacity",
	    sizeof(sc->sc_sensor[3].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[3]);

	sc->sc_sensor[4].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[4].desc, "Battery design voltage",
	    sizeof(sc->sc_sensor[4].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[4]);

	sc->sc_sensor[5].type = SENSOR_AMPS;
	strlcpy(sc->sc_sensor[5].desc, "Battery current",
	    sizeof(sc->sc_sensor[5].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[5]);

	sc->sc_sensor[6].type = SENSOR_VOLTS_DC;
	strlcpy(sc->sc_sensor[6].desc, "Battery voltage",
	    sizeof(sc->sc_sensor[6].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[6]);

	sc->sc_sensor[7].type = SENSOR_TEMP;
	strlcpy(sc->sc_sensor[7].desc, "Battery temperature",
	    sizeof(sc->sc_sensor[7].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[7]);

	sc->sc_sensor[8].type = SENSOR_PERCENT;
	strlcpy(sc->sc_sensor[8].desc, "Battery capacity",
	    sizeof(sc->sc_sensor[8].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[8]);

	sc->sc_sensor[9].type = SENSOR_INDICATOR;
	strlcpy(sc->sc_sensor[9].desc, "Battery charging",
	    sizeof(sc->sc_sensor[9].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[9]);

	sc->sc_sensor[10].type = SENSOR_INDICATOR;
	strlcpy(sc->sc_sensor[10].desc, "AC-Power",
	    sizeof(sc->sc_sensor[10].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[10]);

	sc->sc_sensor[11].type = SENSOR_INTEGER;
	strlcpy(sc->sc_sensor[11].desc, "Battery low-level status",
	    sizeof(sc->sc_sensor[11].desc));
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[11]);

	sensordev_install(&sc->sc_sensordev);

	printf("\n");
}

void
ykbec_write(struct ykbec_softc *mcsc, u_int reg, u_int datum)
{
	struct ykbec_softc *sc = (struct ykbec_softc *)mcsc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, 0, (reg >> 8) & 0xff);
	bus_space_write_1(iot, ioh, 1, (reg >> 0) & 0xff);
	bus_space_write_1(iot, ioh, 2, datum);
}

u_int
ykbec_read(struct ykbec_softc *mcsc, u_int reg)
{
	struct ykbec_softc *sc = (struct ykbec_softc *)mcsc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, 0, (reg >> 8) & 0xff);
	bus_space_write_1(iot, ioh, 1, (reg >> 0) & 0xff);
	return bus_space_read_1(iot, ioh, 2);
}

u_int
ykbec_read16(struct ykbec_softc *mcsc, u_int reg)
{
	u_int val;

	val = ykbec_read(mcsc, reg);
	return (val << 8) | ykbec_read(mcsc, reg + 1);
}

#define KB3310_FAN_SPEED_DIVIDER	480000

#define ECTEMP_CURRENT_REG		0xf458
#define REG_FAN_SPEED_HIGH		0xfe22
#define REG_FAN_SPEED_LOW		0xfe23

#define REG_DESIGN_CAP_HIGH		0xf77d
#define REG_DESIGN_CAP_LOW		0xf77e
#define REG_FULLCHG_CAP_HIGH		0xf780
#define REG_FULLCHG_CAP_LOW		0xf781

#define REG_DESIGN_VOL_HIGH		0xf782
#define REG_DESIGN_VOL_LOW		0xf783
#define REG_CURRENT_HIGH		0xf784
#define REG_CURRENT_LOW			0xf785
#define REG_VOLTAGE_HIGH		0xf786
#define REG_VOLTAGE_LOW			0xf787
#define REG_TEMPERATURE_HIGH		0xf788
#define REG_TEMPERATURE_LOW		0xf789
#define REG_RELATIVE_CAT_HIGH		0xf492
#define REG_RELATIVE_CAT_LOW		0xf493
#define REG_BAT_VENDOR			0xf4c4
#define REG_BAT_CELL_COUNT		0xf4c6

#define REG_BAT_CHARGE			0xf4a2
#define BAT_CHARGE_AC			0x00
#define BAT_CHARGE_DISCHARGE		0x01
#define BAT_CHARGE_CHARGE		0x02

#define REG_POWER_FLAG			0xf440
#define POWER_FLAG_ADAPTER_IN		(1<<0)
#define POWER_FLAG_POWER_ON		(1<<1)
#define POWER_FLAG_ENTER_SUS		(1<<2)

#define REG_BAT_STATUS			0xf4b0
#define BAT_STATUS_BAT_EXISTS		(1<<0)
#define BAT_STATUS_BAT_FULL		(1<<1)
#define BAT_STATUS_BAT_DESTROY		(1<<2)
#define BAT_STATUS_BAT_LOW		(1<<5)

#define REG_CHARGE_STATUS		0xf4b1
#define CHARGE_STATUS_PRECHARGE		(1<<1)
#define CHARGE_STATUS_OVERHEAT		(1<<2)

#define REG_BAT_STATE			0xf482
#define BAT_STATE_DISCHARGING		(1<<0)
#define BAT_STATE_CHARGING		(1<<1)

void
ykbec_refresh(void *arg)
{
	struct ykbec_softc *sc = (struct ykbec_softc *)arg;
	u_int val, bat_charge, bat_status, charge_status, bat_state, power_flag;
	int current;

	val = ykbec_read16(sc, REG_FAN_SPEED_HIGH) & 0xfffff;
	if (val != 0)
		val = KB3310_FAN_SPEED_DIVIDER / val;
	else
		val = UINT_MAX;
	sc->sc_sensor[0].value = val;

	val = ykbec_read(sc, ECTEMP_CURRENT_REG);
	sc->sc_sensor[1].value = val * 1000000 + 273150000;

	sc->sc_sensor[2].value = ykbec_read16(sc, REG_DESIGN_CAP_HIGH) * 1000;
	sc->sc_sensor[3].value = ykbec_read16(sc, REG_FULLCHG_CAP_HIGH) * 1000;
	sc->sc_sensor[4].value = ykbec_read16(sc, REG_DESIGN_VOL_HIGH) * 1000;

	current = ykbec_read16(sc, REG_CURRENT_HIGH);
	/* sign extend short -> int, int -> int64 will be done next statement */
	current |= -(current & 0x8000);
	sc->sc_sensor[5].value = current * -1000;

	sc->sc_sensor[6].value = ykbec_read16(sc, REG_VOLTAGE_HIGH) * 1000;

	val = ykbec_read16(sc, REG_TEMPERATURE_HIGH);
	sc->sc_sensor[7].value = val * 1000000 + 273150000;

	sc->sc_sensor[8].value = ykbec_read16(sc, REG_RELATIVE_CAT_HIGH) * 1000;

	bat_charge = ykbec_read(sc, REG_BAT_CHARGE);
	bat_status = ykbec_read(sc, REG_BAT_STATUS);
	charge_status = ykbec_read(sc, REG_CHARGE_STATUS);
	bat_state = ykbec_read(sc, REG_BAT_STATE);
	power_flag = ykbec_read(sc, REG_POWER_FLAG);

	sc->sc_sensor[9].value = (bat_state & BAT_STATE_CHARGING) ? 1 : 0;
	sc->sc_sensor[10].value = (power_flag & POWER_FLAG_ADAPTER_IN) ? 1 : 0;
	sc->sc_sensor[11].value = (bat_state << 24) | (charge_status << 16) |
	    (bat_status << 8) | bat_charge;
}
