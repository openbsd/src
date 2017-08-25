/*	$OpenBSD: rktemp.c,v 1.1 2017/08/25 10:29:54 kettenis Exp $	*/
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
#define TSADC_USER_CON		0x0000
#define TSADC_AUTO_CON		0x0004
#define  TSADC_AUTO_CON_TSHUT_POLARITY	(1 << 8)
#define  TSADC_AUTO_CON_SRC1_EN		(1 << 5)
#define  TSADC_AUTO_CON_SRC0_EN		(1 << 4)
#define  TSADC_AUTO_CON_TSADC_Q_SEL	(1 << 1)
#define  TSADC_AUTO_CON_AUTO_EN		(1 << 0)
#define TSADC_INT_EN		0x0008
#define  TSADC_INT_EN_TSHUT_2CRU_EN_SRC1	(1 << 9)
#define  TSADC_INT_EN_TSHUT_2CRU_EN_SRC0	(1 << 8)
#define  TSADC_INT_EN_TSHUT_2GPIO_EN_SRC1	(1 << 5)
#define  TSADC_INT_EN_TSHUT_2GPIO_EN_SRC0	(1 << 4)
#define TSADC_INT_PD		0x000c
#define TSADC_DATA0		0x0020
#define TSADC_DATA1		0x0024
#define TSADC_COMP0_INT		0x0030
#define TSADC_COMP1_INT		0x0034
#define TSADC_COMP0_SHUT	0x0040
#define TSADC_COMP1_SHUT	0x0044
#define TSADC_AUTO_PERIOD	0x0068
#define TSADC_AUTO_PERIOD_HT	0x006c

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct rktemp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct ksensor		sc_sensors[2];
	struct ksensordev	sc_sensordev;
};

int	rktemp_match(struct device *, void *, void *);
void	rktemp_attach(struct device *, struct device *, void *);

struct cfattach	rktemp_ca = {
	sizeof (struct rktemp_softc), rktemp_match, rktemp_attach
};

struct cfdriver rktemp_cd = {
	NULL, "rktemp", DV_DULL
};

uint32_t rktemp_calc_code(int32_t);
int32_t rktemp_calc_temp(uint32_t);
int	rktemp_valid(uint32_t);
void	rktemp_refresh_sensors(void *);

int
rktemp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "rockchip,rk3399-tsadc"));
}

void
rktemp_attach(struct device *parent, struct device *self, void *aux)
{
	struct rktemp_softc *sc = (struct rktemp_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint32_t mode, polarity, temp;
	uint32_t auto_con, int_en;
	int node = faa->fa_node;

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

	pinctrl_byname(node, "init");

	clock_enable(node, "tsadc");
	clock_enable(node, "apb_pclk");

	/* Reset the TS-ADC controller block. */
	reset_assert(node, "tsadc-apb");
	delay(10);
	reset_deassert(node, "tsadc-apb");

	mode = OF_getpropint(node, "rockchip,hw-tshut-mode", 1);
	polarity = OF_getpropint(node, "rockchip,hw-tshut-polarity", 0);
	temp = OF_getpropint(node, "rockchip,hw-tshut-temp", 95000);

	auto_con = HREAD4(sc, TSADC_AUTO_CON);
	auto_con |= TSADC_AUTO_CON_TSADC_Q_SEL;
	if (polarity)
		auto_con |= TSADC_AUTO_CON_TSHUT_POLARITY;
	HWRITE4(sc, TSADC_AUTO_CON, auto_con);

	/* Configure mode. */
	int_en = HREAD4(sc, TSADC_INT_EN);
	if (mode) {
		int_en |= TSADC_INT_EN_TSHUT_2GPIO_EN_SRC0;
		int_en |= TSADC_INT_EN_TSHUT_2GPIO_EN_SRC1;
	} else {
		int_en |= TSADC_INT_EN_TSHUT_2CRU_EN_SRC0;
		int_en |= TSADC_INT_EN_TSHUT_2CRU_EN_SRC1;
	}
	HWRITE4(sc, TSADC_INT_EN, int_en);

	/* Set shutdown limit. */
	HWRITE4(sc, TSADC_COMP0_SHUT, rktemp_calc_code(temp));
	auto_con |= TSADC_AUTO_CON_SRC0_EN;
	HWRITE4(sc, TSADC_COMP1_SHUT, rktemp_calc_code(temp));
	auto_con |= TSADC_AUTO_CON_SRC1_EN;
	HWRITE4(sc, TSADC_AUTO_CON, auto_con);

	pinctrl_byname(faa->fa_node, "default");

	/* Finally turn on the ADC. */
	auto_con |= TSADC_AUTO_CON_AUTO_EN;
	HWRITE4(sc, TSADC_AUTO_CON, auto_con);

	/* Register sensors. */
	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));
	strlcpy(sc->sc_sensors[0].desc, "CPU", sizeof(sc->sc_sensors[0].desc));
	sc->sc_sensors[0].type = SENSOR_TEMP;
	sc->sc_sensors[0].flags = SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[0]);
	strlcpy(sc->sc_sensors[1].desc, "GPU", sizeof(sc->sc_sensors[1].desc));
	sc->sc_sensors[1].type = SENSOR_TEMP;
	sc->sc_sensors[1].flags = SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[1]);
	sensordev_install(&sc->sc_sensordev);
	sensor_task_register(sc, rktemp_refresh_sensors, 5);
}

struct rktemp_table_entry {
	int32_t temp;
	uint32_t code;
};

/* RK3399 conversion table. */
struct rktemp_table_entry rktemp_table[] = {
	{ -40000, 402 },
	{ -35000, 410 },
	{ -30000, 419 },
	{ -25000, 427 },
	{ -20000, 436 },
	{ -15000, 444 },
	{ -10000, 453 },
	{  -5000, 461 },
	{      0, 470 },
	{   5000, 478 },
	{  10000, 487 },
	{  15000, 496 },
	{  20000, 504 },
	{  25000, 513 },
	{  30000, 521 },
	{  35000, 530 },
	{  40000, 538 },
	{  45000, 547 },
	{  50000, 555 },
	{  55000, 564 },
	{  60000, 573 },
	{  65000, 581 },
	{  70000, 590 },
	{  75000, 599 },
	{  80000, 607 },
	{  85000, 616 },
	{  90000, 624 },
	{  95000, 633 },
	{ 100000, 642 },
	{ 105000, 650 },
	{ 110000, 659 },
	{ 115000, 668 },
	{ 120000, 677 },
	{ 125000, 685 }
};

uint32_t
rktemp_calc_code(int32_t temp)
{
	const int n = nitems(rktemp_table);
	uint32_t code0, delta_code;
	int32_t temp0, delta_temp;
	int i;

	if (temp <= rktemp_table[0].temp)
		return rktemp_table[0].code;
	if (temp >= rktemp_table[n - 1].temp)
		return rktemp_table[n - 1].code;

	for (i = 1; i < n; i++) {
		if (temp < rktemp_table[i].temp)
			break;
	}

	code0 = rktemp_table[i - 1].code;
	temp0 = rktemp_table[i - 1].temp;
	delta_code = rktemp_table[i].code - code0;
	delta_temp = rktemp_table[i].temp - temp0;

	return code0 + (temp - temp0) * delta_code / delta_temp;
}

int32_t
rktemp_calc_temp(uint32_t code)
{
	const int n = nitems(rktemp_table);
	uint32_t code0, delta_code;
	int32_t temp0, delta_temp;
	int i;

	if (code <= rktemp_table[0].code)
		return rktemp_table[0].temp;
	if (code >= rktemp_table[n - 1].code)
		return rktemp_table[n - 1].temp;

	for (i = 1; i < n; i++) {
		if (code < rktemp_table[i].code)
			break;
	}

	code0 = rktemp_table[i - 1].code;
	temp0 = rktemp_table[i - 1].temp;
	delta_code = rktemp_table[i].code - code0;
	delta_temp = rktemp_table[i].temp - temp0;

	return temp0 + (code - code0) * delta_temp / delta_code;
}

int
rktemp_valid(uint32_t code)
{
	const int n = nitems(rktemp_table);

	if (code < rktemp_table[0].code)
		return 0;
	if (code > rktemp_table[n - 1].code)
		return 0;
	return 1;
}

void
rktemp_refresh_sensors(void *arg)
{
	struct rktemp_softc *sc = arg;
	uint32_t code;

	code = HREAD4(sc, TSADC_DATA0);
	sc->sc_sensors[0].value = 1000 * rktemp_calc_temp(code) + 273150000;
	if (rktemp_valid(code))
		sc->sc_sensors[0].flags &= ~SENSOR_FINVALID;
	else
		sc->sc_sensors[0].flags |= SENSOR_FINVALID;

	code = HREAD4(sc, TSADC_DATA1);
	sc->sc_sensors[1].value = 1000 * rktemp_calc_temp(code) + 273150000;
	if (rktemp_valid(code))
		sc->sc_sensors[1].flags &= ~SENSOR_FINVALID;
	else
		sc->sc_sensors[1].flags |= SENSOR_FINVALID;
}
