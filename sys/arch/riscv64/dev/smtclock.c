/*	$OpenBSD: smtclock.c,v 1.1 2026/04/05 11:40:50 kettenis Exp $	*/
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

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* APBC clocks */
#define K1_CLK_UART0		0
#define K1_CLK_UART2		1
#define K1_CLK_UART3		2
#define K1_CLK_UART4		3
#define K1_CLK_UART5		4
#define K1_CLK_UART6		5
#define K1_CLK_UART7		6
#define K1_CLK_UART8		7
#define K1_CLK_UART9		8

/* APMU resets */
#define K1_RESET_UART0		0
#define K1_RESET_UART2		1
#define K1_RESET_UART3		2
#define K1_RESET_UART4		3
#define K1_RESET_UART5		4
#define K1_RESET_UART6		5
#define K1_RESET_UART7		6
#define K1_RESET_UART8		7
#define K1_RESET_UART9		8

/* APMU clocks */
#define K1_CLK_USB30		16
#define K1_CLK_PCIE0_MASTER	28
#define K1_CLK_PCIE0_SLAVE	29
#define K1_CLK_PCIE0_DBI	30

/* APMU resets */
#define K1_RESET_USB30_AHB	8
#define K1_RESET_USB30_VCC	9
#define K1_RESET_USB30_PHY	10
#define K1_RESET_PCIE0_MASTER	23
#define K1_RESET_PCIE0_SLAVE	24
#define K1_RESET_PCIE0_DBI	25
#define K1_RESET_PCIE0_GLOBAL	26

/* APBC registers */
#define APBC_UART1_CLK_RST		0x0000
#define APBC_UART2_CLK_RST		0x0004
#define APBC_UART3_CLK_RST		0x0024
#define APBC_UART4_CLK_RST		0x0070
#define APBC_UART5_CLK_RST		0x0074
#define APBC_UART6_CLK_RST		0x0078
#define APBC_UART7_CLK_RST		0x0094
#define APBC_UART8_CLK_RST		0x0098
#define APBC_UART9_CLK_RST		0x009c
#define  APBC_UARTX_CLK_RST_FNCLKSEL(x)	(((x) >> 4) & 0x7)

/* APMU registers */
#define APMU_USB_CLK_RES_CTRL		0x005c
#define APMU_PCIE_CLK_RES_CTRL_PORTA	0x03cc

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct smtclock {
	int8_t		idx;
	uint16_t	reg;
	int8_t		bit;
};

struct smtreset {
	int8_t		idx;
	uint16_t	reg;
	int8_t		assert_bit;
	int8_t		deassert_bit;
};

static struct smtclock k1_apbc_clocks[] = {
	{ K1_CLK_UART0, APBC_UART1_CLK_RST, 1 },
	{ K1_CLK_UART2, APBC_UART2_CLK_RST, 1 },
	{ K1_CLK_UART3, APBC_UART3_CLK_RST, 1 },
	{ K1_CLK_UART4, APBC_UART4_CLK_RST, 1 },
	{ K1_CLK_UART5, APBC_UART5_CLK_RST, 1 },
	{ K1_CLK_UART6, APBC_UART6_CLK_RST, 1 },
	{ K1_CLK_UART7, APBC_UART7_CLK_RST, 1 },
	{ K1_CLK_UART8, APBC_UART8_CLK_RST, 1 },
	{ K1_CLK_UART9, APBC_UART9_CLK_RST, 1 },
	{ -1 },
};

static struct smtreset k1_apbc_resets[] = {
	{ K1_RESET_UART0, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART2, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART3, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART4, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART5, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART6, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART7, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART8, APBC_UART1_CLK_RST, 2, -1 },
	{ K1_RESET_UART9, APBC_UART1_CLK_RST, 2, -1 },
	{ -1 },
};

static struct smtclock k1_apmu_clocks[] = {
	{ K1_CLK_USB30, APMU_USB_CLK_RES_CTRL, 8 },
	{ K1_CLK_PCIE0_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTA, 2 },
	{ K1_CLK_PCIE0_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTA, 1 },
	{ K1_CLK_PCIE0_DBI, APMU_PCIE_CLK_RES_CTRL_PORTA, 0 },
	{ -1 },
};

static struct smtreset k1_apmu_resets[] = {
	{ K1_RESET_USB30_AHB, APMU_USB_CLK_RES_CTRL, -1, 9 },
	{ K1_RESET_USB30_VCC, APMU_USB_CLK_RES_CTRL, -1, 10 },
	{ K1_RESET_USB30_PHY, APMU_USB_CLK_RES_CTRL, -1, 11 },
	{ K1_RESET_PCIE0_MASTER, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 5 },
	{ K1_RESET_PCIE0_SLAVE, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 4 },
	{ K1_RESET_PCIE0_DBI, APMU_PCIE_CLK_RES_CTRL_PORTA, -1, 3 },
	{ K1_RESET_PCIE0_GLOBAL, APMU_PCIE_CLK_RES_CTRL_PORTA, 8, -1 },
	{ -1 },
};

struct smtclock_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;

	const struct smtclock	*sc_clocks;
	const struct smtreset	*sc_resets;

	struct clock_device	sc_cd;
	struct reset_device	sc_rd;
};

int	smtclock_match(struct device *, void *, void *);
void	smtclock_attach(struct device *, struct device *, void *);

const struct cfattach smtclock_ca = {
	sizeof (struct smtclock_softc), smtclock_match, smtclock_attach
};

struct cfdriver smtclock_cd = {
	NULL, "smtclock", DV_DULL
};

uint32_t smtclock_get_frequency(void *, uint32_t *);
int	smtclock_set_frequency(void *, uint32_t *, uint32_t);
void	smtclock_enable(void *, uint32_t *, int);
void	smtclock_reset(void *, uint32_t *, int);

int
smtclock_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apbc") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apmu");
}

void
smtclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtclock_softc *sc = (struct smtclock_softc *)self;
	struct fdt_attach_args *faa = aux;

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
	regmap_register(sc->sc_node, sc->sc_iot, sc->sc_ioh,
	    faa->fa_reg[0].size);

	if (OF_is_compatible(faa->fa_node, "spacemit,k1-syscon-apbc")) {
		sc->sc_clocks = k1_apbc_clocks;
		sc->sc_resets = k1_apbc_resets;
	} else {
		sc->sc_clocks = k1_apmu_clocks;
		sc->sc_resets = k1_apmu_resets;
	}

	printf("\n");

	sc->sc_cd.cd_node = faa->fa_node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_get_frequency = smtclock_get_frequency;
	sc->sc_cd.cd_set_frequency = smtclock_set_frequency;
	sc->sc_cd.cd_enable = smtclock_enable;
	clock_register(&sc->sc_cd);

	sc->sc_rd.rd_node = faa->fa_node;
	sc->sc_rd.rd_cookie = sc;
	sc->sc_rd.rd_reset = smtclock_reset;
	reset_register(&sc->sc_rd);
}

uint32_t
smtclock_get_frequency(void *cookie, uint32_t *cells)
{
	struct smtclock_softc *sc = cookie;
	const struct smtclock *clock;
	uint32_t idx = cells[0];
	uint32_t reg;

	for (clock = sc->sc_clocks; clock->idx != -1; clock++) {
		if (clock->idx == idx)
			break;
	}

	if (clock->idx == -1)
		goto fail;

	if (OF_is_compatible(sc->sc_node, "spacemit,k1-syscon-apbc")) {
		switch (idx) {
		case K1_CLK_UART0:
		case K1_CLK_UART2:
		case K1_CLK_UART3:
		case K1_CLK_UART4:
		case K1_CLK_UART5:
		case K1_CLK_UART6:
		case K1_CLK_UART7:
		case K1_CLK_UART8:
		case K1_CLK_UART9:
			reg = HREAD4(sc, clock->reg);
			switch (APBC_UARTX_CLK_RST_FNCLKSEL(reg)) {
			case 0:
				return 57600000;
			case 1:
				return 14745600;
			case 2:
				return 48000000;
			}
			break;
		}
	}

fail:
	printf("%s: 0x%08x\n", __func__, idx);
	return 0;
}

int
smtclock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	uint32_t idx = cells[0];

	printf("%s: 0x%08x\n", __func__, idx);
	return -1;
}

void
smtclock_enable(void *cookie, uint32_t *cells, int on)
{
	struct smtclock_softc *sc = cookie;
	const struct smtclock *clock;
	uint32_t idx = cells[0];

	for (clock = sc->sc_clocks; clock->idx != -1; clock++) {
		if (clock->idx == idx)
			break;
	}

	if (clock->idx == -1) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	if (on)
		HSET4(sc, clock->reg, (1U << clock->bit));
	else
		HCLR4(sc, clock->reg, (1U << clock->bit));
}

void
smtclock_reset(void *cookie, uint32_t *cells, int assert)
{
	struct smtclock_softc *sc = cookie;
	const struct smtreset *reset;
	uint32_t idx = cells[0];
	uint32_t assert_mask = 0;
	uint32_t deassert_mask = 0;
	uint32_t mask, val;

	for (reset = sc->sc_resets; reset->idx != -1; reset++) {
		if (reset->idx == idx)
			break;
	}

	if (reset->idx == -1) {
		printf("%s: 0x%08x\n", __func__, idx);
		return;
	}

	if (reset->assert_bit != -1)
		assert_mask = (1U << reset->assert_bit);
	if (reset->deassert_bit != -1)
		deassert_mask = (1U << reset->deassert_bit);

	mask = assert_mask | deassert_mask;
	val = HREAD4(sc, reset->reg) & ~mask;
	if (assert)
		val |= assert_mask;
	else
		val |= deassert_mask;
	HWRITE4(sc, reset->reg, val);
}
