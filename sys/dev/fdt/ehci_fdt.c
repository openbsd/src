/*	$OpenBSD: ehci_fdt.c,v 1.4 2018/08/06 10:52:30 patrick Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_regulator.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

struct ehci_fdt_softc {
	struct ehci_softc	sc;
	int			sc_node;
	void			*sc_ih;
};

int	ehci_fdt_match(struct device *, void *, void *);
void	ehci_fdt_attach(struct device *, struct device *, void *);
int	ehci_fdt_detach(struct device *, int);

struct cfattach ehci_fdt_ca = {
	sizeof(struct ehci_fdt_softc), ehci_fdt_match, ehci_fdt_attach,
	ehci_fdt_detach, ehci_activate
};

void	ehci_init_phys(struct ehci_fdt_softc *);

int
ehci_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "generic-ehci");
}

void
ehci_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct ehci_fdt_softc *sc = (struct ehci_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	char *devname = sc->sc.sc_bus.bdev.dv_xname;
	usbd_status r;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;
	sc->sc.sc_size = faa->fa_reg[0].size;

	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc.ioh)) {
		printf(": can't map registers\n");
		goto out;
	}

	pinctrl_byname(sc->sc_node, "default");

	clock_enable_all(sc->sc_node);
	reset_deassert_all(sc->sc_node);

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_USB,
	    ehci_intr, &sc->sc, devname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		clock_disable_all(sc->sc_node);
		goto unmap;
	}

	printf("\n");

	ehci_init_phys(sc);

	strlcpy(sc->sc.sc_vendor, "Generic", sizeof(sc->sc.sc_vendor));
	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);
		clock_disable_all(sc->sc_node);
		goto disestablish_intr;
	}

	/* Attach usb device. */
	config_found(self, &sc->sc.sc_bus, usbctlprint);
	return;

disestablish_intr:
	fdt_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;
unmap:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
	sc->sc.sc_size = 0;
out:
	return;
}

int
ehci_fdt_detach(struct device *self, int flags)
{
	struct ehci_fdt_softc *sc = (struct ehci_fdt_softc *)self;
	int rv;

	rv = ehci_detach(self, flags);
	if (rv)
		return rv;

	if (sc->sc_ih != NULL) {
		fdt_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

	clock_disable_all(sc->sc_node);
	return 0;
}

struct ehci_phy {
	const char *compat;
	void (*init)(struct ehci_fdt_softc *, uint32_t *);
};

void sun4i_phy_init(struct ehci_fdt_softc *, uint32_t *);
void sun9i_phy_init(struct ehci_fdt_softc *, uint32_t *);

struct ehci_phy ehci_phys[] = {
	{ "allwinner,sun4i-a10-usb-phy", sun4i_phy_init },
	{ "allwinner,sun5i-a13-usb-phy", sun4i_phy_init },
	{ "allwinner,sun6i-a31-usb-phy", sun4i_phy_init },
	{ "allwinner,sun7i-a20-usb-phy", sun4i_phy_init },
	{ "allwinner,sun8i-a23-usb-phy", sun4i_phy_init },
	{ "allwinner,sun8i-a33-usb-phy", sun4i_phy_init },
	{ "allwinner,sun8i-h3-usb-phy", sun4i_phy_init },
	{ "allwinner,sun8i-r40-usb-phy", sun4i_phy_init },
	{ "allwinner,sun8i-v3s-usb-phy", sun4i_phy_init },
	{ "allwinner,sun50i-a64-usb-phy", sun4i_phy_init },
	{ "allwinner,sun9i-a80-usb-phy", sun9i_phy_init },
};

uint32_t *
ehci_next_phy(uint32_t *cells)
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
ehci_init_phy(struct ehci_fdt_softc *sc, uint32_t *cells)
{
	int node;
	int i;

	node = OF_getnodebyphandle(cells[0]);
	if (node == 0)
		return;

	for (i = 0; i < nitems(ehci_phys); i++) {
		if (OF_is_compatible(node, ehci_phys[i].compat)) {
			ehci_phys[i].init(sc, cells);
			return;
		}
	}
}

void
ehci_init_phys(struct ehci_fdt_softc *sc)
{
	uint32_t *phys;
	uint32_t *phy;
	int len, idx;

	idx = OF_getindex(sc->sc_node, "usb", "phy-names");
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
			ehci_init_phy(sc, phy);
			free(phys, M_TEMP, len);
			return;
		}

		phy = ehci_next_phy(phy);
		idx--;
	}
	free(phys, M_TEMP, len);
}

/*
 * Allwinner PHYs.
 */

/* Registers */
#define SUNXI_HCI_ICR		0x800
#define  SUNXI_ULPI_BYPASS	(1 << 0)
#define  SUNXI_AHB_INCRX_ALIGN	(1 << 8)
#define  SUNXI_AHB_INCR4	(1 << 9)
#define  SUNXI_AHB_INCR8	(1 << 10)
#define  SUNXI_AHB_INCR16	(1 << 11)

void
sun4i_phy_init(struct ehci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t vbus_supply;
	char name[32];
	uint32_t val;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	if (node == -1)
		return;

	val = bus_space_read_4(sc->sc.iot, sc->sc.ioh, SUNXI_HCI_ICR);
	val |= SUNXI_AHB_INCR8 | SUNXI_AHB_INCR4;
	val |= SUNXI_AHB_INCRX_ALIGN;
	val |= SUNXI_ULPI_BYPASS;
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, SUNXI_HCI_ICR, val);

	/*
	 * We need to poke an undocumented register to make the PHY
	 * work on Allwinner A64/H3/H5/R40.
	 */
	if (OF_is_compatible(node, "allwinner,sun8i-h3-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun8i-r40-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun50i-a64-usb-phy")) {
		val = bus_space_read_4(sc->sc.iot, sc->sc.ioh, 0x810);
		val &= ~(1 << 1);
		bus_space_write_4(sc->sc.iot, sc->sc.ioh, 0x810, val);
	}

	pinctrl_byname(node, "default");

	/*
	 * On sun4i, sun5i and sun7i, there is a single clock.  The
	 * more recent SoCs have a separate clock for each PHY.
	 */
	if (OF_is_compatible(node, "allwinner,sun4i-a10-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun5i-a13-usb-phy") ||
	    OF_is_compatible(node, "allwinner,sun7i-a20-usb-phy")) {
		clock_enable(node, "usb_phy");
	} else {
		snprintf(name, sizeof(name), "usb%d_phy", cells[1]);
		clock_enable(node, name);
	}

	snprintf(name, sizeof(name), "usb%d_reset", cells[1]);
	reset_deassert(node, name);

	snprintf(name, sizeof(name), "usb%d_vbus-supply", cells[1]);
	vbus_supply = OF_getpropint(node, name, 0);
	if (vbus_supply)
		regulator_enable(vbus_supply);
}

void
sun9i_phy_init(struct ehci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t phy_supply;
	uint32_t val;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	if (node == -1)
		return;

	pinctrl_byname(node, "default");
	clock_enable(node, "phy");
	reset_deassert(node, "phy");

	val = bus_space_read_4(sc->sc.iot, sc->sc.ioh, SUNXI_HCI_ICR);
	val |= SUNXI_AHB_INCR16 | SUNXI_AHB_INCR8 | SUNXI_AHB_INCR4;
	val |= SUNXI_AHB_INCRX_ALIGN;
	val |= SUNXI_ULPI_BYPASS;
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, SUNXI_HCI_ICR, val);

	phy_supply = OF_getpropint(node, "phy-supply", 0);
	if (phy_supply)
		regulator_enable(phy_supply);
}
