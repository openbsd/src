/*	$OpenBSD: smtpinctrl.c,v 1.2 2026/07/26 18:44:22 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define MFPR_PULL_SEL		(1U << 15)
#define MFPR_PULLUP_EN		(1U << 14)
#define MFPR_PULLDN_EN		(1U << 13)
#define MFPR_DRIVE_MASK_K1	(0x7 << 10)
#define MFPR_DRIVE_SHIFT_K1	10
#define MFPR_DRIVE_MASK_K3	(0xf << 9)
#define MFPR_DRIVE_SHIFT_K3	9
#define MFPR_SPU		(1U << 3)
#define MFPR_AF_SEL_MASK	(0x7 << 0)
#define MFPR_AF_SEL_SHIFT	0

#define IO_PWR_DOMAIN_GPIO2_K1	0x080c
#define IO_PWR_DOMAIN_GPIO3_K1	0x0810
#define IO_PWR_DOMAIN_MMC_K1	0x081c
#define IO_PWR_DOMAIN_QSPI_K1	0x0820

#define IO_PWR_DOMAIN_GPIO1_K3	0x0804
#define IO_PWR_DOMAIN_GPIO2_K3	0x080c
#define IO_PWR_DOMAIN_GPIO4_K3	0x0820
#define IO_PWR_DOMAIN_GPIO5_K3	0x0810
#define IO_PWR_DOMAIN_MMC_K3	0x081c
#define IO_PWR_DOMAIN_QSPI_K3	0x082c
#define  IO_PWR_DOMAIN_V18EN	(1U << 2)

#define APBC_ASFAR		0x0050
#define  APBC_ASFAR_KEY		0xbaba
#define APBC_ASSAR		0x0054
#define  APBC_ASSAR_KEY		0xeb10

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct smtpinctrl_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct regmap		*sc_apbc;
	int			sc_node;

	void			(*sc_config_pin)(struct smtpinctrl_softc *,
				    uint32_t, int, int, int);
};

static int8_t k1_drive_strength_1v8[] = {
	11, -1, 21, -1, 32, -1, 42, -1
};

static int8_t k1_drive_strength_3v3[] = {
	7, 10, 13, 16, 19, 23, 26, 29
};

static int8_t k3_drive_strength_1v8[] = {
	2, 4, 6, 7, 9, 11, 13, 14,
	21, 23, 25, 26, 28, 30, 31, 33
};

static int8_t k3_drive_strength_3v3[] = {
	3, 5, 7, 9, 11, 13, 15, 17,
	25, 27,	29, 31, 33, 35, 37, 38
};

int	smtpinctrl_match(struct device *, void *, void *);
void	smtpinctrl_attach(struct device *, struct device *, void *);

const struct cfattach smtpinctrl_ca = {
	sizeof (struct smtpinctrl_softc), smtpinctrl_match, smtpinctrl_attach
};

struct cfdriver smtpinctrl_cd = {
	NULL, "smtpinctrl", DV_DULL
};

static void k1_config_pin(struct smtpinctrl_softc *, uint32_t, int, int, int);
static void k3_config_pin(struct smtpinctrl_softc *, uint32_t, int, int, int);

int	smtpinctrl_pinctrl(uint32_t, void *);

int
smtpinctrl_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-pinctrl") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k3-pinctrl");
}

void
smtpinctrl_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtpinctrl_softc *sc = (struct smtpinctrl_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint32_t apbc;

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
	sc->sc_node = faa->fa_node;

	printf("\n");

	clock_enable_all(sc->sc_node);

	apbc = OF_getpropint(faa->fa_node, "spacemit,apbc", 0);
	sc->sc_apbc = regmap_byphandle(apbc);

	if (OF_is_compatible(faa->fa_node, "spacemit,k1-pinctrl"))
		sc->sc_config_pin = k1_config_pin;
	else
		sc->sc_config_pin = k3_config_pin;
	
	pinctrl_register(sc->sc_node, smtpinctrl_pinctrl, sc);
}

static void
k1_config_pin(struct smtpinctrl_softc *sc, uint32_t pinmux,
    int bias, int ds, int ps)
{
	uint16_t func = pinmux & 0xffff;
	uint16_t pin = pinmux >> 16;
	bus_size_t offset = -1;
	bus_size_t pd_offset = -1;
	uint32_t val, pd_val;
	int i;

	if (pin <= 85)
		offset = (pin + 1) * 4;
	if (pin >= 93 && pin <= 97)
		offset = (pin + 24) * 4;

	if (pin >= 47 && pin <= 52)
		pd_offset = IO_PWR_DOMAIN_GPIO3_K1;
	if (pin >= 75 && pin <= 80)
		pd_offset = IO_PWR_DOMAIN_GPIO2_K3;
	if (pin >= 98 && pin <= 103)
		pd_offset = IO_PWR_DOMAIN_QSPI_K1;
	if (pin >= 104 && pin <= 109)
		pd_offset = IO_PWR_DOMAIN_MMC_K1;

	if (offset == -1) {
		printf("%s: unsupported pin %d\n", sc->sc_dev.dv_xname, pin);
		return;
	}

	val = HREAD4(sc, offset);

	/* Select function */
	val &= ~MFPR_AF_SEL_MASK;
	val |= (func << MFPR_AF_SEL_SHIFT);

	/* Set bias */
	if (bias != -1) {
		val &= ~(MFPR_PULL_SEL | MFPR_PULLUP_EN | MFPR_PULLDN_EN);
		val &= ~MFPR_SPU;
		val |= bias;
	}

	/* Set drive strength */
	if (ds != -1) {
		/* Require power source for switchable pins. */
		if (pd_offset != -1 && ps == -1) {
			printf("%s: missing power source for pin %d\n",
			   sc->sc_dev.dv_xname, pin);
			return;
		}

		/* Set power domain */
		if (ps != -1 && pd_offset != -1) {
			if (ps != 1800 && ps != 3300) {
				printf("%s: unsupported power source %d\n",
				    sc->sc_dev.dv_xname, ps);
				return;
			}
			if (sc->sc_apbc == NULL) {
				printf("%s: can't access protected registers\n",
				    sc->sc_dev.dv_xname);
				return;
			}
			pd_val = (ps == 3300) ? 0 : IO_PWR_DOMAIN_V18EN;
			regmap_write_4(sc->sc_apbc,
			    APBC_ASFAR, APBC_ASFAR_KEY);
			regmap_write_4(sc->sc_apbc,
			    APBC_ASSAR, APBC_ASSAR_KEY);
			HWRITE4(sc, pd_offset, pd_val);
		}

		/* Translate from mA to register value. */
		if (ps == -1 || ps == 1800) {
			for (i = 0; i < nitems(k1_drive_strength_1v8); i++) {
				if (k1_drive_strength_1v8[i] == ds) {
					ds = i;
					break;
				}
			}
			if (i == nitems(k1_drive_strength_1v8)) {
				printf("%s: unsupported drive strength %d\n",
				       sc->sc_dev.dv_xname, ds);
				return;
			}
		} else if (ps == 3300) {
			for (i = 0; i < nitems(k1_drive_strength_3v3); i++) {
				if (k1_drive_strength_3v3[i] == ds) {
					ds = i;
					break;
				}
			}
			if (i == nitems(k1_drive_strength_3v3)) {
				printf("%s: unsupported drive strength %d\n",
				       sc->sc_dev.dv_xname, ds);
				return;
			}
		}
		val &= ~MFPR_DRIVE_MASK_K1;
		val |= (ds << MFPR_DRIVE_SHIFT_K1);
	}

	HWRITE4(sc, offset, val);
}

static void
k3_config_pin(struct smtpinctrl_softc *sc, uint32_t pinmux,
    int bias, int ds, int ps)
{
	uint16_t func = pinmux & 0xffff;
	uint16_t pin = pinmux >> 16;
	bus_size_t offset = -1;
	bus_size_t pd_offset = -1;
	uint32_t val, pd_val;
	int i;

	if (pin <= 130)
		offset = pin * 4;
	if (pin >= 131 && pin <= 152)
		offset = (pin + 2) * 4;

	if (pin <= 20)
		pd_offset = IO_PWR_DOMAIN_GPIO1_K3;
	if (pin >= 21 && pin <= 41)
		pd_offset = IO_PWR_DOMAIN_GPIO2_K3;
	if (pin >= 76 && pin <= 98)
		pd_offset = IO_PWR_DOMAIN_GPIO4_K3;
	if (pin >= 99 && pin <= 127)
		pd_offset = IO_PWR_DOMAIN_GPIO5_K3;
	if (pin >= 132 && pin <= 137)
		pd_offset = IO_PWR_DOMAIN_MMC_K3;
	if (pin >= 138 && pin <= 144)
		pd_offset = IO_PWR_DOMAIN_QSPI_K3;

	if (offset == -1) {
		printf("%s: unsupported pin %d\n", sc->sc_dev.dv_xname, pin);
		return;
	}

	val = HREAD4(sc, offset);

	/* Select function */
	val &= ~MFPR_AF_SEL_MASK;
	val |= (func << MFPR_AF_SEL_SHIFT);

	/* Set bias */
	if (bias != -1) {
		val &= ~(MFPR_PULL_SEL | MFPR_PULLUP_EN | MFPR_PULLDN_EN);
		val &= ~MFPR_SPU;
		val |= bias;
	}

	/* Set drive strength */
	if (ds != -1) {
		/* Require power source for switchable pins. */
		if (pd_offset != -1 && ps == -1) {
			printf("%s: missing power source for pin %d\n",
			   sc->sc_dev.dv_xname, pin);
			return;
		}

		/* Set power domain */
		if (ps != -1 && pd_offset != -1) {
			if (ps != 1800 && ps != 3300) {
				printf("%s: unsupported power source %d\n",
				    sc->sc_dev.dv_xname, ps);
				return;
			}
			if (sc->sc_apbc == NULL) {
				printf("%s: can't access protected registers\n",
				    sc->sc_dev.dv_xname);
				return;
			}
			pd_val = (ps == 3300) ? 0 : IO_PWR_DOMAIN_V18EN;
			regmap_write_4(sc->sc_apbc,
			    APBC_ASFAR, APBC_ASFAR_KEY);
			regmap_write_4(sc->sc_apbc,
			    APBC_ASSAR, APBC_ASSAR_KEY);
			HWRITE4(sc, pd_offset, pd_val);
		}

		/* Translate from mA to register value. */
		if (ps == -1 || ps == 1800) {
			for (i = 0; i < nitems(k3_drive_strength_1v8); i++) {
				if (k3_drive_strength_1v8[i] == ds) {
					ds = i;
					break;
				}
			}
			if (i == nitems(k3_drive_strength_1v8)) {
				printf("%s: unsupported drive strength %d\n",
				       sc->sc_dev.dv_xname, ds);
				return;
			}
		} else if (ps == 3300) {
			for (i = 0; i < nitems(k3_drive_strength_3v3); i++) {
				if (k3_drive_strength_3v3[i] == ds) {
					ds = i;
					break;
				}
			}
			if (i == nitems(k3_drive_strength_3v3)) {
				printf("%s: unsupported drive strength %d\n",
				       sc->sc_dev.dv_xname, ds);
				return;
			}
		}
		val &= ~MFPR_DRIVE_MASK_K3;
		val |= (ds << MFPR_DRIVE_SHIFT_K3);
	}

	HWRITE4(sc, offset, val);
}

int
smtpinctrl_pinctrl(uint32_t phandle, void *cookie)
{
	struct smtpinctrl_softc *sc = cookie;
	uint32_t *pinmux;
	int node, child;
	int bias, ds, ps;
	int i, len;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return -1;

	for (child = OF_child(node); child; child = OF_peer(child)) {
		len = OF_getproplen(child, "pinmux");
		if (len <= 0)
			continue;

		if (OF_getproplen(child, "input-schmitt") >= 0) {
			printf("%s: \"input-schmitt\" unsupported\n",
			    sc->sc_dev.dv_xname);
			continue;
		}
		if (OF_getproplen(child, "slew-rate") >= 0) {
			printf("%s: \"slew-rate\" unsupported\n",
			    sc->sc_dev.dv_xname);
			continue;
		}

		/* Bias */
		bias = OF_getpropint(child, "bias-pull-up", -1);
		if (bias == -1) {
			if (OF_getpropbool(child, "bias-pull-down"))
				bias = MFPR_PULL_SEL | MFPR_PULLDN_EN;
			else if (OF_getpropbool(child, "bias-disable"))
				bias = 0;
		} else {
			if (bias == 1)
				bias = MFPR_SPU;
			bias |= MFPR_PULL_SEL | MFPR_PULLUP_EN;
		}

		/* Drive strength */
		ds = OF_getpropint(child, "drive-strength", -1);

		/* Power source */
		ps = OF_getpropint(child, "power-source", -1);

		pinmux = malloc(len, M_TEMP, M_WAITOK);
		OF_getpropintarray(child, "pinmux", pinmux, len);

		for (i = 0; i < len / sizeof(uint32_t); i++)
			sc->sc_config_pin(sc, pinmux[i], bias, ds, ps);

		free(pinmux, M_TEMP, len);
	}

	return -1;
}
