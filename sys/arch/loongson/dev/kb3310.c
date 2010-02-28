/*	$OpenBSD: kb3310.c,v 1.5 2010/02/28 08:30:27 otto Exp $	*/
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

#include <machine/apmvar.h>
#include <machine/bus.h>
#include <dev/isa/isavar.h>

#include "apm.h"

struct cfdriver ykbec_cd = {
	NULL, "ykbec", DV_DULL,
};

#define IO_YKBEC		0x381
#define IO_YKBECSIZE		0x3

static const struct {
	const char *desc;	
	int type;
} ykbec_table[] = {
#define YKBEC_FAN	0
	{ NULL,				SENSOR_FANRPM },
#define YKBEC_ITEMP	1
	{ "Internal temperature",	SENSOR_TEMP },
#define YKBEC_DCAP	2
	{ "Battery design capacity",	SENSOR_AMPHOUR },
#define YKBEC_FCAP	3
	{ "Battery full charge capacity", SENSOR_AMPHOUR },
#define YKBEC_DVOLT	4
	{ "Battery design voltage",	SENSOR_VOLTS_DC },
#define YKBEC_BCURRENT	5
	{ "Battery current", 		SENSOR_AMPS },
#define YKBEC_BVOLT	6
	{ "Battery voltage",		SENSOR_VOLTS_DC },
#define YKBEC_BTEMP	7
	{ "Battery temperature",	SENSOR_TEMP },
#define YKBEC_CAP	8
	{ "Battery capacity", 		SENSOR_PERCENT },
#define YKBEC_CHARGING	9
	{ "Battery charging",		SENSOR_INDICATOR },
#define YKBEC_AC	10
	{ "AC-Power",			SENSOR_INDICATOR }
#define YKBEC_NSENSORS	11
};

struct ykbec_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct ksensor		sc_sensor[YKBEC_NSENSORS];
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

#if NAPM > 0
int	ykbec_apminfo(struct apm_power_info *);
struct apm_power_info ykbec_apmdata;
const char *ykbec_batstate[] = {
	"high",
	"low",
	"critical",
	"charging",
	"unknown"
};
#define BATTERY_STRING(x) ((x) < nitems(ykbec_batstate) ? \
	ykbec_batstate[x] : ykbec_batstate[4])
#endif


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
	int i;

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

	for (i = 0; i < YKBEC_NSENSORS; i++) {
		sc->sc_sensor[i].type = ykbec_table[i].type; 
		if (ykbec_table[i].desc) 
			strlcpy(sc->sc_sensor[i].desc, ykbec_table[i].desc,
			    sizeof(sc->sc_sensor[i].desc));
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensor[i]);
	}

	sensordev_install(&sc->sc_sensordev);

#if NAPM > 0
	apm_setinfohook(ykbec_apminfo);
#endif
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
	u_int cap_pct, fullcap;
	int current;
#if NAPM > 0
	struct apm_power_info old;
#endif

	val = ykbec_read16(sc, REG_FAN_SPEED_HIGH) & 0xfffff;
	if (val != 0) {
		val = KB3310_FAN_SPEED_DIVIDER / val;
		sc->sc_sensor[YKBEC_FAN].value = val;
		sc->sc_sensor[YKBEC_FAN].flags &= ~SENSOR_FINVALID;
	} else
		sc->sc_sensor[YKBEC_FAN].flags |= SENSOR_FINVALID;

	val = ykbec_read(sc, ECTEMP_CURRENT_REG);
	sc->sc_sensor[YKBEC_ITEMP].value = val * 1000000 + 273150000;

	sc->sc_sensor[YKBEC_DCAP].value = ykbec_read16(sc, REG_DESIGN_CAP_HIGH)
	    * 1000;
	fullcap = ykbec_read16(sc, REG_FULLCHG_CAP_HIGH);
	sc->sc_sensor[YKBEC_FCAP].value = fullcap * 1000;
	sc->sc_sensor[YKBEC_DVOLT].value = ykbec_read16(sc, REG_DESIGN_VOL_HIGH)
	    * 1000;

	current = ykbec_read16(sc, REG_CURRENT_HIGH);
	/* sign extend short -> int, int -> int64 will be done next statement */
	current |= -(current & 0x8000);
	sc->sc_sensor[YKBEC_BCURRENT].value = -1000 * current;

	sc->sc_sensor[YKBEC_BVOLT].value = ykbec_read16(sc, REG_VOLTAGE_HIGH) *
	    1000;

	val = ykbec_read16(sc, REG_TEMPERATURE_HIGH);
	sc->sc_sensor[YKBEC_BTEMP].value = val * 1000000 + 273150000;

	cap_pct = ykbec_read16(sc, REG_RELATIVE_CAT_HIGH);
	sc->sc_sensor[YKBEC_CAP].value = cap_pct * 1000;

	bat_charge = ykbec_read(sc, REG_BAT_CHARGE);
	bat_status = ykbec_read(sc, REG_BAT_STATUS);
	charge_status = ykbec_read(sc, REG_CHARGE_STATUS);
	bat_state = ykbec_read(sc, REG_BAT_STATE);
	power_flag = ykbec_read(sc, REG_POWER_FLAG);

	sc->sc_sensor[YKBEC_CHARGING].value = (bat_state & BAT_STATE_CHARGING) ?
	    1 : 0;
	sc->sc_sensor[YKBEC_AC].value = (power_flag & POWER_FLAG_ADAPTER_IN) ?
	    1 : 0;

#if NAPM > 0
	bcopy(&ykbec_apmdata, &old, sizeof(old));
	ykbec_apmdata.battery_life = cap_pct;
	ykbec_apmdata.ac_state = (power_flag & POWER_FLAG_ADAPTER_IN) ?
	    APM_AC_ON : APM_AC_OFF;
	if ((bat_status & BAT_STATUS_BAT_EXISTS) == 0) {
		ykbec_apmdata.battery_state = APM_BATTERY_ABSENT;
		ykbec_apmdata.minutes_left = 0;
		ykbec_apmdata.battery_life = 0;
	} else {
		/* if charging, return the minutes until full */
		if (bat_state & BAT_STATE_CHARGING) {
			ykbec_apmdata.battery_state = APM_BATT_CHARGING;
			if (current > 0) {
				fullcap = (100 - cap_pct) * 60 * fullcap / 100;
				ykbec_apmdata.minutes_left = fullcap / current;
			} else
				ykbec_apmdata.minutes_left = 0;
		} else {
			/* arbitrary */
			if (cap_pct > 60)
				ykbec_apmdata.battery_state = APM_BATT_HIGH;
			else if (cap_pct < 10)
				ykbec_apmdata.battery_state = APM_BATT_CRITICAL;
			else
				ykbec_apmdata.battery_state = APM_BATT_LOW;

			current = -current;
			/* Yeeloong draw is about 1A */
			if (current <= 0)
				current = 1000;
			/* at 5?%, the Yeeloong shuts down */
			if (cap_pct <= 5)
				cap_pct = 0;
			else
				cap_pct -= 5;
			fullcap = cap_pct * 60 * fullcap / 100;
			ykbec_apmdata.minutes_left = fullcap / current;
		}

	}
	if (old.ac_state != ykbec_apmdata.ac_state) 
		apm_record_event(APM_POWER_CHANGE, "AC power",
			ykbec_apmdata.ac_state ? "restored" : "lost");
	if (old.battery_state != ykbec_apmdata.battery_state) 
		apm_record_event(APM_POWER_CHANGE, "battery",
		    BATTERY_STRING(ykbec_apmdata.battery_state));
#endif
}


#if NAPM > 0

int
ykbec_apminfo(struct apm_power_info *info)
{
	 bcopy(&ykbec_apmdata, info, sizeof(struct apm_power_info));
	 return 0;
}

#endif
