/*	$OpenBSD: smtcomphy.c,v 1.4 2026/08/14 19:50:28 kettenis Exp $	*/
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

/* PHY registers */
#define PCIE_PU_ADDR_CLK_CFG(_lane)	(0x0008 + (_lane) * 0x0400)
#define  PLL_READY			(1U << 0)
#define  CFG_INTERNAL_TIMER_ADJ_MASK	(0xf << 7)
#define  CFG_INTERNAL_TIMER_ADJ_USB3	(0x2 << 7)
#define  CFG_INTERNAL_TIMER_ADJ_PCIE	(0x6 << 7)
#define  CFG_SW_PHY_INIT_DONE		(1U << 11)
#define PCIE_RC_DONE_STATUS		0x0018
#define  CFG_FORCE_RCV_RETRY		(1U << 10)
#define PCIE_RC_CAL_REG2(_lane)		(0x0020 + (_lane) * 0x0400)
#define  RC_CAL_TOGGLE			(1U << 22)
#define  CLKSEL_MASK			(0x7 << 29)
#define  CLKSEL_24M			(0x3 << 29)
#define PCIE_PU_PLL_1			0x0048
#define  REF_100_WSSC			(1U << 12)
#define  FREF_SEL_MASK			(0x7 << 13)
#define  FREF_SEL_24M			(0x1 << 13)
#define  SSC_DEP_SEL_MASK		(0xf << 16)
#define  SSC_DEP_SEL_NONE		(0x0 << 16)
#define  SSC_DEP_SEL_5000PPM		(0xa << 16)
#define PCIE_PU_PLL_2			0x004c
#define  GEN_REF100			(1U << 4)
#define PCIE_RX_REG1(_lane)		(0x0050 + (_lane) * 0x0400)
#define  AFE_RTERM_REG_MASK		(0xf << 8)
#define  AFE_RTERM_REG_SHIFT		8
#define  EN_RTERM			(1U << 3)
#define PCIE_RX_REG2(_lane)		(0x0054 + (_lane) * 0x0400)
#define  RX_RTERM_SEL			(1U << 5)
#define PCIE_LTSSM_DIS_ENTRY(_lane)	(0x005c + (_lane) * 0x0400)
#define  CFG_REFCLK_MODE_MASK		(0x3 << 8)
#define  CFG_REFCLK_MODE_DRIVER		(0x1 << 8)
#define  OVRD_REFCLK_MODE		(1U << 10)
#define PCIE_TX_REG1(_lane)		(0x0064 + (_lane) * 0x0400)
#define  TX_RTERM_REG_MASK		(0xf << 12)
#define  TX_RTERM_REG_SHIFT		12
#define  TX_RTERM_SEL			(1U << 25)
#define USB3_TEST_CTRL			0x0068
#define PCIE_RCAL_RESULT		0x0084
#define  RTERM_VALUE_RX(_val)		(((_val) >> 0) & 0xf)
#define  RTERM_VALUE_TX(_val)		(((_val) >> 4) & 0xf)
#define  R_TUNE_DONE			(1U << 10)

/* APMU registers */
#define APMU_PMUA_USB_PHY_CTRL0		0x0110
#define  APMU_COMBO_PHY_SEL		(1U << 3)
#define APMU_PCIE_CLK_RES_CTRL_PORTA	0x03cc
#define  APMU_PCIE_APP_HOLD_PHY_RST	(1U << 30)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

/* Calibration values (shared amongst ports). */
uint32_t rx_rterm;
uint32_t tx_rterm;

struct smtcomphy_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct regmap		*sc_apmu;
	struct regmap		*sc_apb_spare;

	int			sc_node;
	int			sc_num_lanes;

	struct phy_device	sc_pd;
};

int smtcomphy_match(struct device *, void *, void *);
void smtcomphy_attach(struct device *, struct device *, void *);

const struct cfattach smtcomphy_ca = {
	sizeof (struct smtcomphy_softc), smtcomphy_match, smtcomphy_attach
};

struct cfdriver smtcomphy_cd = {
	NULL, "smtcomphy", DV_DULL
};

int	smtcomphy_combo_init(struct smtcomphy_softc *sc);
int	smtcomphy_combo_enable(void *, uint32_t *);
int	smtcomphy_pcie_enable(void *, uint32_t *);
int	smtcomphy_k3_combo_init(struct smtcomphy_softc *sc);
int	smtcomphy_k3_combo_enable(void *, uint32_t *);

int
smtcomphy_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-combo-phy") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k1-pcie-phy") ||
	    OF_is_compatible(faa->fa_node, "spacemit,k3-combo-phy");
}

void
smtcomphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtcomphy_softc *sc = (struct smtcomphy_softc *)self;
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

	printf("\n");

	reset_deassert(sc->sc_node, "phy");

	if (OF_is_compatible(sc->sc_node, "spacemit,k1-combo-phy")) {
		if (smtcomphy_combo_init(sc))
			return;
		sc->sc_num_lanes = 1;
	} else if (OF_is_compatible(sc->sc_node, "spacemit,k3-combo-phy")) {
		if (smtcomphy_k3_combo_init(sc))
			return;
		sc->sc_num_lanes = 0;
	} else if (OF_is_compatible(sc->sc_node, "spacemit,k1-pcie-phy")) {
		sc->sc_num_lanes = OF_getpropint(sc->sc_node, "num-lanes", 2);
	}

	sc->sc_pd.pd_node = sc->sc_node;
	sc->sc_pd.pd_cookie = sc;
	if (OF_is_compatible(faa->fa_node, "spacemit,k1-combo-phy"))
		sc->sc_pd.pd_enable = smtcomphy_combo_enable;
	else if (OF_is_compatible(sc->sc_node, "spacemit,k3-combo-phy"))
		sc->sc_pd.pd_enable = smtcomphy_k3_combo_enable;
	else if (OF_is_compatible(sc->sc_node, "spacemit,k1-pcie-phy"))
		sc->sc_pd.pd_enable = smtcomphy_pcie_enable;
	phy_register(&sc->sc_pd);
}

int
smtcomphy_combo_init(struct smtcomphy_softc *sc)
{
	uint32_t apmu, val;
	int timo;

	apmu = OF_getpropint(sc->sc_node, "spacemit,apmu", 0);
	sc->sc_apmu = regmap_byphandle(apmu);
	if (sc->sc_apmu == NULL) {
		printf("%s: can't get apmu\n", sc->sc_dev.dv_xname);
		return -1;
	}

	val = regmap_read_4(sc->sc_apmu, APMU_PCIE_CLK_RES_CTRL_PORTA);
	val &= ~APMU_PCIE_APP_HOLD_PHY_RST;
	regmap_write_4(sc->sc_apmu, APMU_PCIE_CLK_RES_CTRL_PORTA, val);

	val = HREAD4(sc, PCIE_RCAL_RESULT);
	if ((val & R_TUNE_DONE) == 0) {
		/*
		 * Firmware didn't do calibration, so give it a go
		 * ourselves.
		 */

		/* Need to be in PCIe for calibration. */
		val = regmap_read_4(sc->sc_apmu, APMU_PMUA_USB_PHY_CTRL0);
		val &= ~APMU_COMBO_PHY_SEL;
		regmap_write_4(sc->sc_apmu, APMU_PMUA_USB_PHY_CTRL0, val);

		clock_enable(sc->sc_node, "dbi");
		clock_enable(sc->sc_node, "mstr");
		clock_enable(sc->sc_node, "slv");
		reset_deassert(sc->sc_node, "dbi");
		reset_deassert(sc->sc_node, "mstr");
		reset_deassert(sc->sc_node, "slv");

		for (timo = 1000; timo > 0; timo--) {
			val = HREAD4(sc, PCIE_RCAL_RESULT);
			if (val & R_TUNE_DONE)
				break;
			delay(500);
		}

		reset_assert(sc->sc_node, "dbi");
		reset_assert(sc->sc_node, "mstr");
		reset_assert(sc->sc_node, "slv");
		clock_disable(sc->sc_node, "dbi");
		clock_disable(sc->sc_node, "mstr");
		clock_disable(sc->sc_node, "slv");
	}
	if (val & R_TUNE_DONE) {
		/*
		 * Save calibration values, such that they can be used
		 * by the other PCIe ports.
		 */
		rx_rterm = RTERM_VALUE_RX(val);
		tx_rterm = RTERM_VALUE_TX(val);
		return 0;
	}

	printf("%s: calibration failed\n", sc->sc_dev.dv_xname);
	return -1;
}

void
smtcomphy_pll_init_common(struct smtcomphy_softc *sc)
{
	uint32_t val;
	int lane, timo;

	val = HREAD4(sc, PCIE_PU_PLL_1);
	val &= ~REF_100_WSSC;
	val &= ~FREF_SEL_MASK;
	val |= FREF_SEL_24M;
	HWRITE4(sc, PCIE_PU_PLL_1, val);

	for (lane = 0; lane < sc->sc_num_lanes; lane++) {
		val = HREAD4(sc, PCIE_PU_ADDR_CLK_CFG(lane));
		val |= CFG_SW_PHY_INIT_DONE;
		HWRITE4(sc, PCIE_PU_ADDR_CLK_CFG(lane), val);
	}

	for (timo = 1000; timo > 0; timo--) {
		if (HREAD4(sc, PCIE_PU_ADDR_CLK_CFG(0)) & PLL_READY)
			break;
		delay(500);
	}
	if (timo == 0)
		printf("%s: PLL lock timeout\n", sc->sc_dev.dv_xname);
}

void
smtcomphy_pll_init_usb3(struct smtcomphy_softc *sc)
{
	uint32_t val;

	val = HREAD4(sc, PCIE_PU_ADDR_CLK_CFG(0));
	val &= ~CFG_INTERNAL_TIMER_ADJ_MASK;
	val |= CFG_INTERNAL_TIMER_ADJ_USB3;
	HWRITE4(sc, PCIE_PU_ADDR_CLK_CFG(0), val);

	val = HREAD4(sc, PCIE_PU_PLL_1);
	val &= ~SSC_DEP_SEL_MASK;
	val |= SSC_DEP_SEL_5000PPM;
	HWRITE4(sc, PCIE_PU_PLL_1, val);

	smtcomphy_pll_init_common(sc);
}

void
smtcomphy_pll_init_pcie(struct smtcomphy_softc *sc)
{
	uint32_t val;
	int lane;

	for (lane = 0; lane < sc->sc_num_lanes; lane++) {
		val = HREAD4(sc, PCIE_PU_ADDR_CLK_CFG(lane));
		val &= ~CFG_INTERNAL_TIMER_ADJ_MASK;
		val |= CFG_INTERNAL_TIMER_ADJ_PCIE;
		HWRITE4(sc, PCIE_PU_ADDR_CLK_CFG(lane), val);
	}

	val = HREAD4(sc, PCIE_RC_DONE_STATUS);
	val |= CFG_FORCE_RCV_RETRY;
	HWRITE4(sc, PCIE_RC_DONE_STATUS, val);

	val = HREAD4(sc, PCIE_PU_PLL_1);
	val &= ~SSC_DEP_SEL_MASK;
	val |= SSC_DEP_SEL_NONE;
	HWRITE4(sc, PCIE_PU_PLL_1, val);

	val = HREAD4(sc, PCIE_PU_PLL_2);
	val |= GEN_REF100;
	HWRITE4(sc, PCIE_PU_PLL_2, val);

	smtcomphy_pll_init_common(sc);
}

int
smtcomphy_combo_enable(void *cookie, uint32_t *cells)
{
	struct smtcomphy_softc *sc = cookie;
	uint32_t type = cells[0];
	uint32_t val, oval;

	/* We only support PCIE and USB3. */
	if (type != PHY_TYPE_PCIE && type != PHY_TYPE_USB3)
		return EINVAL;

	/*
	 * Select the desired function; only change if not already set
	 * to the desired value.
	 */
	val = oval = regmap_read_4(sc->sc_apmu, APMU_PMUA_USB_PHY_CTRL0);
	if (type == PHY_TYPE_USB3)
		val |= APMU_COMBO_PHY_SEL;
	else
		val &= ~APMU_COMBO_PHY_SEL;
	if (val != oval)
		regmap_write_4(sc->sc_apmu, APMU_PMUA_USB_PHY_CTRL0, val);

	if (type == PHY_TYPE_PCIE)
		return smtcomphy_pcie_enable(cookie, cells);

	HWRITE4(sc, USB3_TEST_CTRL, 0);
	smtcomphy_pll_init_usb3(sc);

	return 0;
}

int
smtcomphy_pcie_enable(void *cookie, uint32_t *cells)
{
	struct smtcomphy_softc *sc = cookie;
	uint32_t val;
	int lane;

	for (lane = 0; lane < sc->sc_num_lanes; lane++) {
		val = HREAD4(sc, PCIE_RX_REG1(lane));
		val &= ~AFE_RTERM_REG_MASK;
		val |= rx_rterm << AFE_RTERM_REG_SHIFT;
		val |= EN_RTERM;
		HWRITE4(sc, PCIE_RX_REG1(lane), val);

		val = HREAD4(sc, PCIE_RX_REG2(lane));
		val &= ~RX_RTERM_SEL;
		HWRITE4(sc, PCIE_RX_REG2(lane), val);

		val = HREAD4(sc, PCIE_TX_REG1(lane));
		val &= ~TX_RTERM_REG_MASK;
		val |= tx_rterm << TX_RTERM_REG_SHIFT;
		val |= TX_RTERM_SEL;
		HWRITE4(sc, PCIE_TX_REG1(lane), val);

		val = HREAD4(sc, PCIE_RC_CAL_REG2(lane));
		val &= ~CLKSEL_MASK;
		val |= CLKSEL_24M;
		val &= ~RC_CAL_TOGGLE;
		HWRITE4(sc, PCIE_RC_CAL_REG2(lane), val);
		val |= RC_CAL_TOGGLE;
		HWRITE4(sc, PCIE_RC_CAL_REG2(lane), val);

		val = HREAD4(sc, PCIE_LTSSM_DIS_ENTRY(lane));
		val |= OVRD_REFCLK_MODE;
		val &= ~CFG_REFCLK_MODE_MASK;
		val |= CFG_REFCLK_MODE_DRIVER;
		HWRITE4(sc, PCIE_LTSSM_DIS_ENTRY(lane), val);
	}

	smtcomphy_pll_init_pcie(sc);

	return 0;
}

/* Additional PHY registers for K3.  And some bits that moved around? */
#define PHY_RESET_CFG(_lane)	(0x0004 + (_lane) * 0x0400)
#define PHY_MODE_CFG(_lane)	(0x000c + (_lane) * 0x0400)
#define PHY_PU_SEL(_lane)	(0x0040 + (_lane) * 0x0400)
/* PCIE_RX_REG1 */
#define  REFCLK_MODE_MASK	(0x3 << 0)
#define  REFCLK_MODE_DRIVER	(0x1 << 0)
#define  SEL_TRI_CODE		(1U << 2)
#define  LEGACY_MASK		(0xff << 8)
#define  LEGACY_DEFAULT		(0x65 << 8)
#define PHY_PLL_REG1		0x0058
#define PHY_PLL_REG2		0x005c
#define PHY_RX_REG_A(_lane)	(0x0060 + (_lane) * 0x0400)
#define PHY_RX_REG_B(_lane)	(0x0064 + (_lane) * 0x0400)

/* Additional APMU and APB_SPARE registers for K3. */
#define APMU_PCIE_SUBSYS_MGMT		0x01d8
#define  APMU_PCIE_SUBSYS_MGTM_PCI_USB_COMBO_MODE_MASK (0x1f << 0)
#define APB_SPARE31			0x0178
#define  APB_SPARE31_PU_CAL		(1U << 17)

int
smtcomphy_k3_combo_init(struct smtcomphy_softc *sc)
{
	uint32_t apmu[2] = {};
	uint32_t apb_spare, val;
	int len;

	len = OF_getpropintarray(sc->sc_node, "spacemit,apmu",
	    apmu, sizeof(apmu));
	sc->sc_apmu = regmap_byphandle(apmu[0]);
	if (len != sizeof(apmu) || sc->sc_apmu == NULL) {
		printf("%s: can't get apmu\n", sc->sc_dev.dv_xname);
		return -1;
	}

	apb_spare = OF_getpropint(sc->sc_node, "spacemit,apb-spare", 0);
	sc->sc_apb_spare = regmap_byphandle(apb_spare);
	if (sc->sc_apb_spare == NULL) {
		printf("%s: can't get abp-spare\n", sc->sc_dev.dv_xname);
		return -1;
	}

	if (apmu[1] & ~APMU_PCIE_SUBSYS_MGTM_PCI_USB_COMBO_MODE_MASK) {
		printf("%s: invalid PHY matrix config\n", sc->sc_dev.dv_xname);
		return -1;
	}

	val = regmap_read_4(sc->sc_apmu, APMU_PCIE_SUBSYS_MGMT);
	val &= ~APMU_PCIE_SUBSYS_MGTM_PCI_USB_COMBO_MODE_MASK;
	val |= apmu[1];
	regmap_write_4(sc->sc_apmu, APMU_PCIE_SUBSYS_MGMT, val);

	val = regmap_read_4(sc->sc_apb_spare, APB_SPARE31);
	if ((val & APB_SPARE31_PU_CAL) == 0)
		printf("%s: not calibrated\n", sc->sc_dev.dv_xname);

	switch (apmu[1]) {
	case 0x11:
		break;
	default:
		printf("%s: unsupported PHY matrix config\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}

	return 0;
}


int
smtcomphy_k3_combo_enable(void *cookie, uint32_t *cells)
{
	struct smtcomphy_softc *sc = cookie;
	uint32_t group = cells[0];
	uint32_t type = cells[1];
	bus_size_t offset;
	uint32_t val;
	int num_lanes;
	int lane, timo;

	/* We only support PCIE for now. */
	if (type != PHY_TYPE_PCIE)
		return EINVAL;

	/* There are five lane groups. */
	if (group > 5)
		return EINVAL;

	/* Lane group 0 and 1 have two lanes; all others just one. */
	num_lanes = (group < 2) ? 2 : 1;
	offset = group * 0x100000;

	val = HREAD4(sc, offset + PHY_PLL_REG1);
	val &= ~(0xf << 12);
	val |= (0x2 << 12);
	HWRITE4(sc, offset + PHY_PLL_REG1, val);

	val = HREAD4(sc, offset + PHY_PLL_REG2);
	val &= ~(1U << 21);
	HWRITE4(sc, offset + PHY_PLL_REG2, val);

	for (lane = 0; lane < num_lanes; lane++) {
		val = HREAD4(sc, offset + PCIE_RX_REG1(lane));
		val &= ~REFCLK_MODE_MASK;
		HWRITE4(sc, offset + PCIE_RX_REG1(lane), val);
	}

	val = HREAD4(sc, offset + PHY_PLL_REG2);
	val |= (1U << 20);
	HWRITE4(sc, offset + PHY_PLL_REG2, val);

	val = HREAD4(sc, offset + PCIE_RX_REG1(0));
	val = REFCLK_MODE_DRIVER | SEL_TRI_CODE | LEGACY_DEFAULT;
	HWRITE4(sc, offset + PCIE_RX_REG1(0), val);

	val = HREAD4(sc, offset + PHY_PLL_REG1);
	val &= ~(0xf << 24);
	HWRITE4(sc, offset + PHY_PLL_REG1, val);

	for (lane = 0; lane < num_lanes; lane++) {
		val = HREAD4(sc, offset + PHY_PU_SEL(lane));
		val |= (1U << 13);
		HWRITE4(sc, offset + PHY_PU_SEL(lane), val);

		val = HREAD4(sc, offset + PHY_RESET_CFG(lane));
		val &= ~(1U << 6);
		HWRITE4(sc, offset + PHY_RESET_CFG(lane), val);

		val = HREAD4(sc, offset + PHY_MODE_CFG(lane));
		val |= (1U << 2);
		HWRITE4(sc, offset + PHY_MODE_CFG(lane), val);

		val = HREAD4(sc, offset + PHY_RX_REG_A(lane));
		val = 0xdf987810;
		HWRITE4(sc, offset + PHY_RX_REG_A(lane), val);

		val = HREAD4(sc, offset + PHY_RX_REG_B(lane));
		val &= ~0x00ffffff;
		val |= 0x002888b4;
		HWRITE4(sc, offset + PHY_RX_REG_B(lane), val);

		val = HREAD4(sc, offset + PCIE_PU_ADDR_CLK_CFG(lane));
		val |= CFG_SW_PHY_INIT_DONE;
		HWRITE4(sc, offset + PCIE_PU_ADDR_CLK_CFG(lane), val);
	}

	for (timo = 1000; timo > 0; timo--) {
		if (HREAD4(sc, offset + PCIE_PU_ADDR_CLK_CFG(0)) & PLL_READY)
			break;
		delay(500);
	}
	if (timo == 0)
		printf("%s: PLL lock timeout\n", sc->sc_dev.dv_xname);

	return 0;
}
