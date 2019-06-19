/*	$OpenBSD: sxitemp.c,v 1.4 2018/05/27 21:59:26 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define THS_CTRL0			0x0000
#define  THS_CTRL0_SENSOR_ACQ(x)	((x) & 0xffff)
#define THS_CTRL2			0x0040
#define  THS_CTRL2_ADC_ACQ(x)		(((x) & 0xffff) << 16)
#define  THS_CTRL2_SENSE2_EN		(1 << 2)
#define  THS_CTRL2_SENSE1_EN		(1 << 1)
#define  THS_CTRL2_SENSE0_EN		(1 << 0)
#define THS_INT_CTRL			0x0044
#define  THS_INT_CTRL_THERMAL_PER(x)	(((x) & 0xfffff) << 12)
#define THS_FILTER			0x0070
#define  THS_FILTER_EN			(1 << 2)
#define  THS_FILTER_TYPE(x)		((x) & 0x3) 
#define THS0_DATA			0x0080
#define THS1_DATA			0x0084
#define THS2_DATA			0x0088

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct sxitemp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	uint64_t		(*sc_calc_temp0)(int64_t);
	uint64_t		(*sc_calc_temp1)(int64_t);
	uint64_t		(*sc_calc_temp2)(int64_t);

	struct ksensor		sc_sensors[3];
	struct ksensordev	sc_sensordev;
};

int	sxitemp_match(struct device *, void *, void *);
void	sxitemp_attach(struct device *, struct device *, void *);

struct cfattach	sxitemp_ca = {
	sizeof (struct sxitemp_softc), sxitemp_match, sxitemp_attach
};

struct cfdriver sxitemp_cd = {
	NULL, "sxitemp", DV_DULL
};

uint64_t sxitemp_h3_calc_temp(int64_t);
uint64_t sxitemp_r40_calc_temp(int64_t);
uint64_t sxitemp_a64_calc_temp(int64_t);
uint64_t sxitemp_h5_calc_temp0(int64_t);
uint64_t sxitemp_h5_calc_temp1(int64_t);
void	sxitemp_refresh_sensors(void *);

int
sxitemp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "allwinner,sun8i-h3-ths") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun8i-r40-ths") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun50i-a64-ths") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun50i-h5-ths"));
}

void
sxitemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct sxitemp_softc *sc = (struct sxitemp_softc *)self;
	struct fdt_attach_args *faa = aux;
	int node = faa->fa_node;
	uint32_t enable;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	pinctrl_byname(node, "default");

	clock_enable_all(node);
	reset_deassert_all(node);

	if (OF_is_compatible(faa->fa_node, "allwinner,sun8i-h3-ths")) {
		sc->sc_calc_temp0 = sxitemp_h3_calc_temp;
		enable = THS_CTRL2_SENSE0_EN;
	} else if (OF_is_compatible(faa->fa_node, "allwinner,sun8i-r40-ths")) {
		sc->sc_calc_temp0 = sxitemp_r40_calc_temp;
		sc->sc_calc_temp1 = sxitemp_r40_calc_temp;
		enable = THS_CTRL2_SENSE0_EN | THS_CTRL2_SENSE1_EN;
	} else if (OF_is_compatible(faa->fa_node, "allwinner,sun50i-a64-ths")) {
		sc->sc_calc_temp0 = sxitemp_a64_calc_temp;
		sc->sc_calc_temp1 = sxitemp_a64_calc_temp;
		sc->sc_calc_temp2 = sxitemp_a64_calc_temp;
		enable = THS_CTRL2_SENSE0_EN | THS_CTRL2_SENSE1_EN |
		    THS_CTRL2_SENSE2_EN;
	} else {
		sc->sc_calc_temp0 = sxitemp_h5_calc_temp0;
		sc->sc_calc_temp1 = sxitemp_h5_calc_temp1;
		enable = THS_CTRL2_SENSE0_EN | THS_CTRL2_SENSE1_EN;
	}

	/* Start data acquisition. */
	HWRITE4(sc, THS_FILTER, THS_FILTER_EN | THS_FILTER_TYPE(1));
	HWRITE4(sc, THS_INT_CTRL, THS_INT_CTRL_THERMAL_PER(800));
	HWRITE4(sc, THS_CTRL0, THS_CTRL0_SENSOR_ACQ(31));
	HWRITE4(sc, THS_CTRL2, THS_CTRL2_ADC_ACQ(31) | enable);

	/* Register sensors. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	if (sc->sc_calc_temp0) {
		strlcpy(sc->sc_sensors[0].desc, "CPU",
		    sizeof(sc->sc_sensors[0].desc));
		sc->sc_sensors[0].type = SENSOR_TEMP;
		sc->sc_sensors[0].flags = SENSOR_FINVALID;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[0]);
	}
	if (sc->sc_calc_temp1) {
		strlcpy(sc->sc_sensors[1].desc, "GPU",
		    sizeof(sc->sc_sensors[1].desc));
		sc->sc_sensors[1].type = SENSOR_TEMP;
		sc->sc_sensors[1].flags = SENSOR_FINVALID;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[1]);
	}
	if (sc->sc_calc_temp2) {
		strlcpy(sc->sc_sensors[2].desc, "",
		    sizeof(sc->sc_sensors[2].desc));
		sc->sc_sensors[2].type = SENSOR_TEMP;
		sc->sc_sensors[2].flags = SENSOR_FINVALID;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[2]);
	}
	sensordev_install(&sc->sc_sensordev);
	sensor_task_register(sc, sxitemp_refresh_sensors, 5);
}

uint64_t
sxitemp_h3_calc_temp(int64_t data)
{
	/* From BSP since the H3 Data Sheet isn't accurate. */
	return 217000000 - data * 1000000000 / 8253;
}

uint64_t
sxitemp_r40_calc_temp(int64_t data)
{
	/* From BSP as the R40 User Manual says T.B.D. */
	return -112500 * data + 250000000;
}

uint64_t
sxitemp_a64_calc_temp(int64_t data)
{
	/* From BSP as the A64 User Manual isn't correct. */
	return (2170000000000 - data * 1000000000) / 8560;
}

uint64_t
sxitemp_h5_calc_temp0(int64_t data)
{
	if (data > 0x500)
		return -119100 * data + 223000000;
	else
		return -145200 * data + 259000000;
}

uint64_t
sxitemp_h5_calc_temp1(int64_t data)
{
	if (data > 0x500)
		return -119100 * data + 223000000;
	else
		return -159000 * data + 276000000;
}

void
sxitemp_refresh_sensors(void *arg)
{
	struct sxitemp_softc *sc = arg;
	uint32_t data;

	if (sc->sc_calc_temp0) {
		data = HREAD4(sc, THS0_DATA);
		sc->sc_sensors[0].value = sc->sc_calc_temp0(data) + 273150000;
		sc->sc_sensors[0].flags &= ~SENSOR_FINVALID;
	}

	if (sc->sc_calc_temp1) {
		data = HREAD4(sc, THS1_DATA);
		sc->sc_sensors[1].value = sc->sc_calc_temp1(data) + 273150000;
		sc->sc_sensors[1].flags &= ~SENSOR_FINVALID;
	}

	if (sc->sc_calc_temp2) {
		data = HREAD4(sc, THS2_DATA);
		sc->sc_sensors[2].value = sc->sc_calc_temp2(data) + 273150000;
		sc->sc_sensors[2].flags &= ~SENSOR_FINVALID;
	}
}
