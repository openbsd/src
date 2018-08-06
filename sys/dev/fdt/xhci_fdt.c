/*	$OpenBSD: xhci_fdt.c,v 1.12 2018/08/06 10:52:30 patrick Exp $	*/
/*
 * Copyright (c) 2017 Mark kettenis <kettenis@openbsd.org>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_power.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct xhci_fdt_softc {
	struct xhci_softc	sc;
	int			sc_node;
	bus_space_handle_t	ph_ioh;
	void			*sc_ih;
};

int	xhci_fdt_match(struct device *, void *, void *);
void	xhci_fdt_attach(struct device *, struct device *, void *);

struct cfattach xhci_fdt_ca = {
	sizeof(struct xhci_fdt_softc), xhci_fdt_match, xhci_fdt_attach
};

void	xhci_dwc3_init(struct xhci_fdt_softc *);
void	xhci_init_phys(struct xhci_fdt_softc *);

int
xhci_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "generic-xhci") ||
	    OF_is_compatible(faa->fa_node, "cavium,octeon-7130-xhci") ||
	    OF_is_compatible(faa->fa_node, "snps,dwc3");
}

void
xhci_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct xhci_fdt_softc *sc = (struct xhci_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	int error;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_size = faa->fa_reg[0].size;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;

	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc.ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_USB,
	    xhci_intr, sc, sc->sc.sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	/* Set up power domain */
	power_domain_enable(sc->sc_node);

	/* 
	 * Synopsys Designware USB3 controller needs some extra
	 * attention because of the additional OTG functionality.
	 */
	if (OF_is_compatible(sc->sc_node, "snps,dwc3"))
		xhci_dwc3_init(sc);

	xhci_init_phys(sc);

	strlcpy(sc->sc.sc_vendor, "Generic", sizeof(sc->sc.sc_vendor));
	if ((error = xhci_init(&sc->sc)) != 0) {
		printf("%s: init failed, error=%d\n",
		    sc->sc.sc_bus.bdev.dv_xname, error);
		goto disestablish_ret;
	}

	/* Attach usb device. */
	config_found(self, &sc->sc.sc_bus, usbctlprint);

	/* Now that the stack is ready, config' the HC and enable interrupts. */
	xhci_config(&sc->sc);

	return;

disestablish_ret:
	fdt_intr_disestablish(sc->sc_ih);
unmap:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
}


/*
 * Synopsys Designware USB3 controller.
 */

#define USB3_GCTL		0xc110
#define  USB3_GCTL_PRTCAPDIR_MASK	(0x3 << 12)
#define  USB3_GCTL_PRTCAPDIR_HOST	(0x1 << 12)
#define  USB3_GCTL_PRTCAPDIR_DEVICE	(0x2 << 12)
#define USB3_GUCTL1		0xc11c
#define  USB3_GUCTL1_TX_IPGAP_LINECHECK_DIS	(1 << 28)
#define USB3_GUSB2PHYCFG0	0xc200
#define  USB3_GUSB2PHYCFG0_U2_FREECLK_EXISTS	(1 << 30)
#define  USB3_GUSB2PHYCFG0_USBTRDTIM(n)	((n) << 10)
#define  USB3_GUSB2PHYCFG0_ENBLSLPM	(1 << 8)
#define  USB3_GUSB2PHYCFG0_SUSPENDUSB20	(1 << 6)
#define  USB3_GUSB2PHYCFG0_PHYIF	(1 << 3)

void
xhci_dwc3_init(struct xhci_fdt_softc *sc)
{
	char phy_type[16] = { 0 };
	int node = sc->sc_node;
	uint32_t reg;

	/* We don't support device mode, so always force host mode. */
	reg = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USB3_GCTL);
	reg &= ~USB3_GCTL_PRTCAPDIR_MASK;
	reg |= USB3_GCTL_PRTCAPDIR_HOST;
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USB3_GCTL, reg);

	/* Configure USB2 PHY type and quirks. */
	OF_getprop(node, "phy_type", phy_type, sizeof(phy_type));
	reg = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USB3_GUSB2PHYCFG0);
	reg &= ~USB3_GUSB2PHYCFG0_USBTRDTIM(0xf);
	if (strcmp(phy_type, "utmi_wide") == 0) {
		reg |= USB3_GUSB2PHYCFG0_PHYIF;
		reg |= USB3_GUSB2PHYCFG0_USBTRDTIM(0x5);
	} else {
		reg &= ~USB3_GUSB2PHYCFG0_PHYIF;
		reg |= USB3_GUSB2PHYCFG0_USBTRDTIM(0x9);
	}
	if (OF_getproplen(node, "snps,dis-u2-freeclk-exists-quirk") == 0)
		reg &= ~USB3_GUSB2PHYCFG0_U2_FREECLK_EXISTS;
	if (OF_getproplen(node, "snps,dis_enblslpm_quirk") == 0)
		reg &= ~USB3_GUSB2PHYCFG0_ENBLSLPM;
	if (OF_getproplen(node, "snps,dis_u2_susphy_quirk") == 0)
		reg &= ~USB3_GUSB2PHYCFG0_SUSPENDUSB20;
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USB3_GUSB2PHYCFG0, reg);

	/* Configure USB3 quirks. */
	reg = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USB3_GUCTL1);
	if (OF_getproplen(node, "snps,dis-tx-ipgap-linecheck-quirk") == 0)
		reg |= USB3_GUCTL1_TX_IPGAP_LINECHECK_DIS;
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USB3_GUCTL1, reg);
}

/*
 * PHY initialization.
 */

struct xhci_phy {
	const char *compat;
	void (*init)(struct xhci_fdt_softc *, uint32_t *);
};

void exynos5_usbdrd_init(struct xhci_fdt_softc *, uint32_t *);
void imx8mq_usb_init(struct xhci_fdt_softc *, uint32_t *);
void nop_xceiv_init(struct xhci_fdt_softc *, uint32_t *);

struct xhci_phy xhci_phys[] = {
	{ "fsl,imx8mq-usb-phy", imx8mq_usb_init },
	{ "samsung,exynos5250-usbdrd-phy", exynos5_usbdrd_init },
	{ "samsung,exynos5420-usbdrd-phy", exynos5_usbdrd_init },
	{ "usb-nop-xceiv", nop_xceiv_init },
};

uint32_t *
xhci_next_phy(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#phy-cells", 0);
	return cells + ncells + 1;
}

void
xhci_init_phy(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	int node;
	int i;

	node = OF_getnodebyphandle(cells[0]);
	if (node == 0)
		return;

	for (i = 0; i < nitems(xhci_phys); i++) {
		if (OF_is_compatible(node, xhci_phys[i].compat)) {
			xhci_phys[i].init(sc, cells);
			return;
		}
	}
}

void
xhci_init_phys(struct xhci_fdt_softc *sc)
{
	uint32_t *phys;
	uint32_t *phy;
	uint32_t usb_phy;
	int len, idx;

	/*
	 * Legacy binding; assume there only is a single USB PHY.
	 */
	usb_phy = OF_getpropint(sc->sc_node, "usb-phy", 0);
	if (usb_phy) {
		xhci_init_phy(sc, &usb_phy);
		return;
	}

	/*
	 * Generic PHY binding; only initialize USB 3 PHY for now.
	 */
	idx = OF_getindex(sc->sc_node, "usb3-phy", "phy-names");
	if (idx < 0)
		return;

	len = OF_getproplen(sc->sc_node, "phys");
	if (len <= 0)
		return;

	phys = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(sc->sc_node, "phys", phys, len);

	phy = phys;
	while (phy && phy < phys + (len / sizeof(uint32_t))) {
		if (idx == 0) {
			xhci_init_phy(sc, phy);
			free(phys, M_TEMP, len);
			return;
		}

		phy = xhci_next_phy(phy);
		idx--;
	}
	free(phys, M_TEMP, len);
}

/*
 * Samsung Exynos 5 PHYs.
 */

/* Registers */
#define EXYNOS5_PHYUTMI			0x0008
#define  EXYNOS5_PHYUTMI_OTGDISABLE	(1 << 6)
#define EXYNOS5_PHYCLKRST		0x0010
#define  EXYNOS5_PHYCLKRST_SSC_EN	(1 << 20)
#define  EXYNOS5_PHYCLKRST_REF_SSP_EN	(1 << 19)
#define  EXYNOS5_PHYCLKRST_PORTRESET	(1 << 1)
#define  EXYNOS5_PHYCLKRST_COMMONONN	(1 << 0)
#define EXYNOS5_PHYTEST			0x0028
#define  EXYNOS5_PHYTEST_POWERDOWN_SSP	(1 << 3)
#define  EXYNOS5_PHYTEST_POWERDOWN_HSP	(1 << 2)

/* PMU registers */
#define EXYNOS5_USBDRD0_POWER		0x0704
#define EXYNOS5420_USBDRD1_POWER	0x0708
#define  EXYNOS5_USBDRD_POWER_EN	(1 << 0)

void
exynos5_usbdrd_init(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t phy_reg[2];
	struct regmap *pmurm;
	uint32_t pmureg;
	uint32_t val;
	bus_size_t offset;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	KASSERT(node != 0);

	if (OF_getpropintarray(node, "reg", phy_reg,
	    sizeof(phy_reg)) != sizeof(phy_reg))
		return;

	if (bus_space_map(sc->sc.iot, phy_reg[0],
	    phy_reg[1], 0, &sc->ph_ioh)) {
		printf("%s: can't map PHY registers\n",
		    sc->sc.sc_bus.bdev.dv_xname);
		return;
	}

	/* Power up the PHY block. */
	pmureg = OF_getpropint(node, "samsung,pmu-syscon", 0);
	pmurm = regmap_byphandle(pmureg);
	if (pmurm) {
		node = OF_getnodebyphandle(pmureg);
		if (sc->sc.sc_bus.bdev.dv_unit == 0)
			offset = EXYNOS5_USBDRD0_POWER;
		else
			offset = EXYNOS5420_USBDRD1_POWER;

		val = regmap_read_4(pmurm, offset);
		val |= EXYNOS5_USBDRD_POWER_EN;
		regmap_write_4(pmurm, offset, val);
	}

	/* Initialize the PHY.  Assumes U-Boot has done initial setup. */
	val = bus_space_read_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYTEST);
	CLR(val, EXYNOS5_PHYTEST_POWERDOWN_SSP);
	CLR(val, EXYNOS5_PHYTEST_POWERDOWN_HSP);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYTEST, val);

	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYUTMI,
	    EXYNOS5_PHYUTMI_OTGDISABLE);

	val = bus_space_read_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST);
	SET(val, EXYNOS5_PHYCLKRST_SSC_EN);
	SET(val, EXYNOS5_PHYCLKRST_REF_SSP_EN);
	SET(val, EXYNOS5_PHYCLKRST_COMMONONN);
	SET(val, EXYNOS5_PHYCLKRST_PORTRESET);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST, val);
	delay(10);
	CLR(val, EXYNOS5_PHYCLKRST_PORTRESET);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST, val);
}

/*
 * i.MX8MQ PHYs.
 */

/* Registers */
#define IMX8MQ_PHY_CTRL0			0x0000
#define  IMX8MQ_PHY_CTRL0_REF_SSP_EN			(1 << 2)
#define IMX8MQ_PHY_CTRL1			0x0004
#define  IMX8MQ_PHY_CTRL1_RESET				(1 << 0)
#define  IMX8MQ_PHY_CTRL1_ATERESET			(1 << 3)
#define  IMX8MQ_PHY_CTRL1_VDATSRCENB0			(1 << 19)
#define  IMX8MQ_PHY_CTRL1_VDATDETENB0			(1 << 20)
#define IMX8MQ_PHY_CTRL2			0x0008
#define  IMX8MQ_PHY_CTRL2_TXENABLEN0			(1 << 8)
#define IMX8MQ_PHY_CTRL3			0x000c

void
imx8mq_usb_init(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t phy_reg[4], reg;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	KASSERT(node != 0);

	if (OF_getpropintarray(node, "reg", phy_reg,
	    sizeof(phy_reg)) != sizeof(phy_reg))
		return;

	if (bus_space_map(sc->sc.iot, phy_reg[1],
	    phy_reg[3], 0, &sc->ph_ioh)) {
		printf("%s: can't map PHY registers\n",
		    sc->sc.sc_bus.bdev.dv_xname);
		return;
	}

	clock_set_assigned(node);
	clock_enable_all(node);

	reg = bus_space_read_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL1);
	reg &= ~(IMX8MQ_PHY_CTRL1_VDATSRCENB0 | IMX8MQ_PHY_CTRL1_VDATDETENB0);
	reg |= IMX8MQ_PHY_CTRL1_RESET | IMX8MQ_PHY_CTRL1_ATERESET;
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL1, reg);

	reg = bus_space_read_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL0);
	reg |= IMX8MQ_PHY_CTRL0_REF_SSP_EN;
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL0, reg);

	reg = bus_space_read_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL2);
	reg |= IMX8MQ_PHY_CTRL2_TXENABLEN0;
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL2, reg);

	reg = bus_space_read_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL1);
	reg &= ~(IMX8MQ_PHY_CTRL1_RESET | IMX8MQ_PHY_CTRL1_ATERESET);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, IMX8MQ_PHY_CTRL1, reg);
}

void
nop_xceiv_init(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t vcc_supply;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	KASSERT(node != 0);

	vcc_supply = OF_getpropint(node, "vcc-supply", 0);
	if (vcc_supply)
		regulator_enable(vcc_supply);
}
