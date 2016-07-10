/* $OpenBSD: imxiomuxc.c,v 1.3 2016/07/10 11:46:28 kettenis Exp $ */
/*
 * Copyright (c) 2013 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2016 Mark Kettenus <kettenis@openbsd.org>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxiomuxcvar.h>

#include <dev/ofw/openfirm.h>

/* registers */
#define IOMUXC_GPR1			0x004
#define IOMUXC_GPR8			0x020
#define IOMUXC_GPR12			0x030
#define IOMUXC_GPR13			0x034

/* bits and bytes */
#define IOMUXC_GPR1_REF_SSP_EN					(1 << 16)
#define IOMUXC_GPR1_TEST_POWERDOWN				(1 << 18)

#define IOMUXC_GPR8_PCS_TX_DEEMPH_GEN1				(0 << 0)
#define IOMUXC_GPR8_PCS_TX_DEEMPH_GEN2_3P5DB			(0 << 6)
#define IOMUXC_GPR8_PCS_TX_DEEMPH_GEN2_6DB			(20 << 12)
#define IOMUXC_GPR8_PCS_TX_SWING_FULL				(127 << 18)
#define IOMUXC_GPR8_PCS_TX_SWING_LOW				(127 << 25)

#define IOMUXC_GPR12_LOS_LEVEL_MASK				(0x1f << 4)
#define IOMUXC_GPR12_LOS_LEVEL_9				(9 << 4)
#define IOMUXC_GPR12_APPS_PM_XMT_PME				(1 << 9)
#define IOMUXC_GPR12_APPS_LTSSM_ENABLE				(1 << 10)
#define IOMUXC_GPR12_APPS_INIT_RST				(1 << 11)
#define IOMUXC_GPR12_DEVICE_TYPE_RC				(2 << 12)
#define IOMUXC_GPR12_DEVICE_TYPE_MASK				(3 << 12)
#define IOMUXC_GPR12_APPS_PM_XMT_TURNOFF			(1 << 16)

#define IOMUXC_GPR13_SATA_PHY_1_FAST_EDGE_RATE			(0x00 << 0)
#define IOMUXC_GPR13_SATA_PHY_1_SLOW_EDGE_RATE			(0x02 << 0)
#define IOMUXC_GPR13_SATA_PHY_1_EDGE_RATE_MASK			0x3
#define IOMUXC_GPR13_SATA_PHY_2_1104V				(0x11 << 2)
#define IOMUXC_GPR13_SATA_PHY_3_333DB				(0x00 << 7)
#define IOMUXC_GPR13_SATA_PHY_4_9_16				(0x04 << 11)
#define IOMUXC_GPR13_SATA_PHY_5_SS				(0x01 << 14)
#define IOMUXC_GPR13_SATA_SPEED_3G				(0x01 << 15)
#define IOMUXC_GPR13_SATA_PHY_6					(0x03 << 16)
#define IOMUXC_GPR13_SATA_PHY_7_SATA2M				(0x12 << 19)
#define IOMUXC_GPR13_SATA_PHY_8_30DB				(0x05 << 24)
#define IOMUXC_GPR13_SATA_MASK					0x07FFFFFD

#define IOMUXC_PAD_CTL_SRE_SLOW		(1 << 0)
#define IOMUXC_PAD_CTL_SRE_FAST		(1 << 0)
#define IOMUXC_PAD_CTL_DSE_HIZ		(0 << 3)
#define IOMUXC_PAD_CTL_DSE_240OHM	(1 << 3)
#define IOMUXC_PAD_CTL_DSE_120OHM	(2 << 3)
#define IOMUXC_PAD_CTL_DSE_80OHM	(3 << 3)
#define IOMUXC_PAD_CTL_DSE_60OHM	(4 << 3)
#define IOMUXC_PAD_CTL_DSE_48OHM	(5 << 3)
#define IOMUXC_PAD_CTL_DSE_40OHM	(6 << 3)
#define IOMUXC_PAD_CTL_DSE_34OHM	(7 << 3)
#define IOMUXC_PAD_CTL_SPEED_TBD	(0 << 6)
#define IOMUXC_PAD_CTL_SPEED_LOW	(1 << 6)
#define IOMUXC_PAD_CTL_SPEED_MED	(2 << 6)
#define IOMUXC_PAD_CTL_SPEED_MAX	(3 << 6)
#define IOMUXC_PAD_CTL_ODE_DISABLED	(0 << 11)
#define IOMUXC_PAD_CTL_ODE_ENABLED	(1 << 11)
#define IOMUXC_PAD_CTL_PKE_DISABLED	(0 << 12)
#define IOMUXC_PAD_CTL_PKE_ENABLED	(1 << 12)
#define IOMUXC_PAD_CTL_PUE_KEEP		(0 << 13)
#define IOMUXC_PAD_CTL_PUE_PULL		(1 << 13)
#define IOMUXC_PAD_CTL_PUS_100K_OHM_PD	(0 << 14)
#define IOMUXC_PAD_CTL_PUS_47K_OHM_PU	(1 << 14)
#define IOMUXC_PAD_CTL_PUS_100K_OHM_PU	(2 << 14)
#define IOMUXC_PAD_CTL_PUS_22K_OHM_PU	(3 << 14)
#define IOMUXC_PAD_CTL_HYS_DISABLED	(0 << 16)
#define IOMUXC_PAD_CTL_HYS_ENABLED	(1 << 16)

#define IOMUXC_IMX6Q_I2C_PAD_CTRL	(IOMUXC_PAD_CTL_SRE_FAST | IOMUXC_PAD_CTL_ODE_ENABLED | \
		IOMUXC_PAD_CTL_PKE_ENABLED | IOMUXC_PAD_CTL_PUE_PULL | IOMUXC_PAD_CTL_DSE_40OHM | \
		IOMUXC_PAD_CTL_PUS_100K_OHM_PU | IOMUXC_PAD_CTL_HYS_ENABLED | IOMUXC_PAD_CTL_SPEED_MED)

#define IOMUX_CONFIG_SION		(1 << 4)

#define IMX_PINCTRL_NO_PAD_CTL		(1 << 31)
#define IMX_PINCTRL_SION		(1 << 30)

struct imxiomuxc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct imxiomuxc_softc *imxiomuxc_sc;

void imxiomuxc_attach(struct device *parent, struct device *self, void *args);
int imxiomuxc_pinctrl(uint32_t);

struct cfattach imxiomuxc_ca = {
	sizeof (struct imxiomuxc_softc), NULL, imxiomuxc_attach
};

struct cfdriver imxiomuxc_cd = {
	NULL, "imxiomuxc", DV_DULL
};

void
imxiomuxc_attach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct imxiomuxc_softc *sc = (struct imxiomuxc_softc *) self;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("imxiomuxc_attach: bus_space_map failed!");

	printf("\n");
	imxiomuxc_sc = sc;
}

int
imxiomuxc_pinctrlbyid(int node, int id)
{
	char pinctrl[32];
	uint32_t *phandles;
	int len, i;

	snprintf(pinctrl, sizeof(pinctrl), "pinctrl-%d", id);
	len = OF_getproplen(node, pinctrl);
	if (len <= 0)
		return -1;

	phandles = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, pinctrl, phandles, len);
	for (i = 0; i < len / sizeof(uint32_t); i++)
		imxiomuxc_pinctrl(phandles[i]);
	free(phandles, M_TEMP, len);
	return 0;
}

int
imxiomuxc_pinctrlbyname(int node, const char *config)
{
	char *names;
	char *name;
	char *end;
	int id = 0;
	int len;

	len = OF_getproplen(node, "pinctrl-names");
	if (len <= 0)
		printf("no pinctrl-names\n");

	names = malloc(len, M_TEMP, M_WAITOK);
	OF_getprop(node, "pinctrl-names", names, len);
	end = names + len;
	name = names;
	while (name < end) {
		if (strcmp(name, config) == 0) {
			free(names, M_TEMP, len);
			return imxiomuxc_pinctrlbyid(node, id);
		}
		name += strlen(name) + 1;
		id++;
	}
	free(names, M_TEMP, len);
	return -1;
}

int
imxiomuxc_pinctrl(uint32_t phandle)
{
	struct imxiomuxc_softc *sc = imxiomuxc_sc;
	uint32_t *pins;
	int npins;
	int node;
	int len;
	int i;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	len = OF_getproplen(node, "fsl,pins");
	if (len <= 0)
		return -1;

	pins = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "fsl,pins", pins, len);
	npins = len / (6 * sizeof(uint32_t));

	for (i = 0; i < npins; i++) {
		uint32_t mux_reg = pins[6 * i + 0];
		uint32_t conf_reg = pins[6 * i + 1];
		uint32_t input_reg = pins[6 * i + 2];
		uint32_t mux_val = pins[6 * i + 3];
		uint32_t conf_val = pins[6 * i + 5];
		uint32_t input_val = pins[6 * i + 4];
		uint32_t val;

		/* Set MUX mode. */
		if (conf_val & IMX_PINCTRL_SION)
			mux_val |= IOMUX_CONFIG_SION;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, mux_reg, mux_val);

		/* Set PAD config. */
		if ((conf_val & IMX_PINCTRL_NO_PAD_CTL) == 0)
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    conf_reg, conf_val);

		/* Set input select. */
		if ((input_val >> 24) == 0xff) {
			/*
			 * Magic value used to clear or set specific
			 * bits in the general purpose registers.
			 */
			uint8_t shift = (input_val >> 16) & 0xff;
			uint8_t width = (input_val >> 8) & 0xff;
			uint32_t clr = ((1 << width) - 1) << shift;
			uint32_t set = (input_val & 0xff) << shift;

			val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    input_reg);
			val &= ~clr;
			val |= set;
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    input_reg, val);
		} else if (input_reg != 0) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    input_reg, input_val);
		}
	}

	free(pins, M_TEMP, len);
	return 0;
}

void
imxiomuxc_enable_sata(void)
{
	struct imxiomuxc_softc *sc = imxiomuxc_sc;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR13,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR13) & ~IOMUXC_GPR13_SATA_MASK) |
		IOMUXC_GPR13_SATA_PHY_1_FAST_EDGE_RATE | IOMUXC_GPR13_SATA_PHY_2_1104V |
		IOMUXC_GPR13_SATA_PHY_3_333DB | IOMUXC_GPR13_SATA_PHY_4_9_16 |
		IOMUXC_GPR13_SATA_SPEED_3G | IOMUXC_GPR13_SATA_PHY_6 |
		IOMUXC_GPR13_SATA_PHY_7_SATA2M | IOMUXC_GPR13_SATA_PHY_8_30DB);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR13,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR13) & ~IOMUXC_GPR13_SATA_PHY_1_SLOW_EDGE_RATE) |
		IOMUXC_GPR13_SATA_PHY_1_SLOW_EDGE_RATE);
}

void
imxiomuxc_enable_pcie(void)
{
	struct imxiomuxc_softc *sc = imxiomuxc_sc;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12) & ~IOMUXC_GPR12_APPS_LTSSM_ENABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12) & ~IOMUXC_GPR12_DEVICE_TYPE_MASK) |
		IOMUXC_GPR12_DEVICE_TYPE_RC);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR12) & ~IOMUXC_GPR12_LOS_LEVEL_MASK) |
		IOMUXC_GPR12_LOS_LEVEL_9);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR8,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR8) |
		IOMUXC_GPR8_PCS_TX_DEEMPH_GEN1 | IOMUXC_GPR8_PCS_TX_DEEMPH_GEN2_3P5DB |
		IOMUXC_GPR8_PCS_TX_DEEMPH_GEN2_6DB | IOMUXC_GPR8_PCS_TX_SWING_FULL |
		IOMUXC_GPR8_PCS_TX_SWING_LOW);
}

void
imxiomuxc_pcie_refclk(int enable)
{
	struct imxiomuxc_softc *sc = imxiomuxc_sc;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1) & ~IOMUXC_GPR1_REF_SSP_EN);

	if (enable)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1) | IOMUXC_GPR1_REF_SSP_EN);
}

void
imxiomuxc_pcie_test_powerdown(int enable)
{
	struct imxiomuxc_softc *sc = imxiomuxc_sc;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1) & ~IOMUXC_GPR1_TEST_POWERDOWN);

	if (enable)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, IOMUXC_GPR1) | IOMUXC_GPR1_TEST_POWERDOWN);
}
