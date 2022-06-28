/*	$OpenBSD: mvtemp.c,v 1.3 2022/06/28 23:43:12 naddy Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define TEMP_STAT			0x0000
#define TEMP_CTRL0			0x0000
#define  TEMP_CTRL0_TSEN_TC_TRIM_MASK	0x7
#define  TEMP_CTRL0_TSEN_TC_TRIM_VAL	0x3
#define TEMP_CTRL1			0x0004
#define  TEMP_CTRL1_TSEN_RESET		(1 << 8)

struct mvtemp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_stat_ioh;
	bus_space_handle_t	sc_ctrl_ioh;

	uint32_t		sc_stat_valid;
	int32_t			(*sc_calc_temp)(uint32_t);

	struct ksensor		sc_sensor;
	struct ksensordev	sc_sensordev;
};

int	mvtemp_match(struct device *, void *, void *);
void	mvtemp_attach(struct device *, struct device *, void *);

const struct cfattach	mvtemp_ca = {
	sizeof (struct mvtemp_softc), mvtemp_match, mvtemp_attach
};

struct cfdriver mvtemp_cd = {
	NULL, "mvtemp", DV_DULL
};

struct mvtemp_compat {
	const char *compat;
	uint32_t stat_valid;
	void	(*init)(struct mvtemp_softc *);
	int32_t	(*calc_temp)(uint32_t);
};

void	mvtemp_ap806_init(struct mvtemp_softc *);
int32_t mvtemp_ap806_calc_temp(uint32_t);
void	mvtemp_cp110_init(struct mvtemp_softc *);
int32_t mvtemp_cp110_calc_temp(uint32_t);

const struct mvtemp_compat mvtemp_compat[] = {
	{
		"marvell,armada-ap806-thermal", (1 << 16),
		mvtemp_ap806_init, mvtemp_ap806_calc_temp,
	},
	{
		"marvell,armada-cp110-thermal", (1 << 10),
		mvtemp_cp110_init, mvtemp_cp110_calc_temp,
	}
};

void	mvtemp_refresh_sensors(void *);

int
mvtemp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int i;

	for (i = 0; i < nitems(mvtemp_compat); i++) {
		if (OF_is_compatible(faa->fa_node, mvtemp_compat[i].compat))
			return 1;
	}

	return 0;
}

void
mvtemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct mvtemp_softc *sc = (struct mvtemp_softc *)self;
	struct fdt_attach_args *faa = aux;
	int i;

	if (faa->fa_nreg < 2) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_stat_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_ctrl_ioh)) {
		bus_space_unmap(sc->sc_iot, sc->sc_stat_ioh,
		    faa->fa_reg[0].size);
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	for (i = 0; i < nitems(mvtemp_compat); i++) {
		if (OF_is_compatible(faa->fa_node, mvtemp_compat[i].compat)) {
			break;
		}
	}
	KASSERT(i < nitems(mvtemp_compat));

	mvtemp_compat[i].init(sc);
	sc->sc_stat_valid = mvtemp_compat[i].stat_valid;
	sc->sc_calc_temp = mvtemp_compat[i].calc_temp;

	/* Register sensors. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	sc->sc_sensor.type = SENSOR_TEMP;
	sc->sc_sensor.flags = SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);
	sensordev_install(&sc->sc_sensordev);
	sensor_task_register(sc, mvtemp_refresh_sensors, 5);
}

/* AP806 */

void
mvtemp_ap806_init(struct mvtemp_softc *sc)
{
}

int32_t
mvtemp_ap806_calc_temp(uint32_t stat)
{
	stat = ((stat & 0x3ff) ^ 0x200) - 0x200;
	return (stat * 423000) + 150000000 + 273150000;
}

/* CP110 */

void
mvtemp_cp110_init(struct mvtemp_softc *sc)
{
	uint32_t ctrl;

	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ctrl_ioh, TEMP_CTRL1);
	ctrl |= TEMP_CTRL1_TSEN_RESET;
	bus_space_write_4(sc->sc_iot, sc->sc_ctrl_ioh, TEMP_CTRL1, ctrl);

	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ctrl_ioh, TEMP_CTRL0);
	ctrl &= ~TEMP_CTRL0_TSEN_TC_TRIM_MASK;
	ctrl |= TEMP_CTRL0_TSEN_TC_TRIM_VAL;
	bus_space_write_4(sc->sc_iot, sc->sc_ctrl_ioh, TEMP_CTRL0, ctrl);
}

int32_t
mvtemp_cp110_calc_temp(uint32_t stat)
{
	return ((stat & 0x3ff) * 476100) - 279100000 + 273150000;
}

void
mvtemp_refresh_sensors(void *arg)
{
	struct mvtemp_softc *sc = arg;
	int32_t stat, temp;

	stat = bus_space_read_4(sc->sc_iot, sc->sc_stat_ioh, TEMP_STAT);
	temp = sc->sc_calc_temp(stat);
	sc->sc_sensor.value = temp;
	if ((stat & sc->sc_stat_valid) && temp >= 0)
		sc->sc_sensor.flags &= ~SENSOR_FINVALID;
	else
		sc->sc_sensor.flags |= SENSOR_FINVALID;
}
