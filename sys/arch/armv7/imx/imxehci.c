/*	$OpenBSD: imxehci.c,v 1.15 2016/08/04 15:52:52 kettenis Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/imx/imxccmvar.h>
#include <armv7/imx/imxiomuxcvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

/* usb phy */
#define USBPHY_PWD			0x00
#define USBPHY_CTRL			0x30
#define USBPHY_CTRL_SET			0x34
#define USBPHY_CTRL_CLR			0x38
#define USBPHY_CTRL_TOG			0x3c

#define USBPHY_CTRL_ENUTMILEVEL2	(1 << 14)
#define USBPHY_CTRL_ENUTMILEVEL3	(1 << 15)
#define USBPHY_CTRL_CLKGATE		(1 << 30)
#define USBPHY_CTRL_SFTRST		(1U << 31)

/* ehci */
#define USB_EHCI_OFFSET			0x100

#define EHCI_USBMODE			0xa8

#define EHCI_USBMODE_HOST		(3 << 0)
#define EHCI_PS_PTS_UTMI_MASK	((1 << 25) | (3 << 30))

/* usb non-core */
#define USBNC_USB_OTG_CTRL		0x00
#define USBNC_USB_UH1_CTRL		0x04

#define USBNC_USB_OTG_CTRL_OVER_CUR_POL	(1 << 8)
#define USBNC_USB_OTG_CTRL_OVER_CUR_DIS	(1 << 7)
#define USBNC_USB_UH1_CTRL_OVER_CUR_POL	(1 << 8)
#define USBNC_USB_UH1_CTRL_OVER_CUR_DIS	(1 << 7)

int	imxehci_match(struct device *, void *, void *);
void	imxehci_attach(struct device *, struct device *, void *);
int	imxehci_detach(struct device *, int);

void	imxehci_enable_vbus(uint32_t);

struct imxehci_softc {
	struct ehci_softc	sc;
	void			*sc_ih;
	bus_space_handle_t	uh_ioh;
	bus_space_handle_t	ph_ioh;
	bus_space_handle_t	nc_ioh;
};

struct cfattach imxehci_ca = {
	sizeof (struct imxehci_softc), imxehci_match, imxehci_attach,
	imxehci_detach
};

struct cfdriver imxehci_cd = {
	NULL, "imxehci", DV_DULL
};

int
imxehci_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "fsl,imx27-usb");
}

void
imxehci_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxehci_softc *sc = (struct imxehci_softc *)self;
	struct fdt_attach_args *faa = aux;
	usbd_status r;
	char *devname = sc->sc.sc_bus.bdev.dv_xname;
	uint32_t phy[1], misc[2];
	uint32_t phy_reg[2];
	uint32_t misc_reg[2];
	uint32_t vbus;
	int node;

	if (faa->fa_nreg < 1)
		return;

	if (OF_getpropintarray(faa->fa_node, "fsl,usbphy",
	    phy, sizeof(phy)) != sizeof(phy))
		return;

	if (OF_getpropintarray(faa->fa_node, "fsl,usbmisc",
	    misc, sizeof(misc)) != sizeof(misc))
		return;

	node = OF_getnodebyphandle(phy[0]);
	if (node == 0)
		return;

	if (OF_getpropintarray(node, "reg", phy_reg,
	    sizeof(phy_reg)) != sizeof(phy_reg))
		return;

	node = OF_getnodebyphandle(misc[0]);
	if (node == 0)
		return;

	if (OF_getpropintarray(node, "reg", misc_reg,
	    sizeof(misc_reg)) != sizeof(misc_reg))
		return;

	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;
	sc->sc.sc_size = faa->fa_reg[0].size - USB_EHCI_OFFSET;

	/* Map I/O space */
	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->uh_ioh)) {
		printf(": cannot map mem space\n");
		goto out;
	}
	if (bus_space_subregion(sc->sc.iot, sc->uh_ioh, USB_EHCI_OFFSET,
	    sc->sc.sc_size, &sc->sc.ioh)) {
		printf(": cannot map mem space\n");
		goto mem0;
	}

	if (bus_space_map(sc->sc.iot, phy_reg[0],
	    phy_reg[1], 0, &sc->ph_ioh)) {
		printf(": cannot map mem space\n");
		goto mem1;
	}

	if (bus_space_map(sc->sc.iot, misc_reg[0],
	    misc_reg[1], 0, &sc->nc_ioh)) {
		printf(": cannot map mem space\n");
		goto mem2;
	}

	printf("\n");

	imxiomuxc_pinctrlbyname(faa->fa_node, "default");

	imxccm_enable_usboh3();
	delay(1000);

	/* enable usb bus power */
	vbus = OF_getpropint(faa->fa_node, "vbus-supply", 0);
	if (vbus)
		imxehci_enable_vbus(vbus);

	switch (misc[1]) {
	case 0:
		/* disable the carger detection, else signal on DP will be poor */
		imxccm_disable_usb1_chrg_detect();
		/* power host 0 */
		imxccm_enable_pll_usb1();

		/* over current and polarity setting */
		bus_space_write_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_OTG_CTRL,
		    bus_space_read_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_OTG_CTRL) |
		    (USBNC_USB_OTG_CTRL_OVER_CUR_POL | USBNC_USB_OTG_CTRL_OVER_CUR_DIS));
		break;
	case 1:
		/* disable the carger detection, else signal on DP will be poor */
		imxccm_disable_usb2_chrg_detect();
		/* power host 1 */
		imxccm_enable_pll_usb2();

		/* over current and polarity setting */
		bus_space_write_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_UH1_CTRL,
		    bus_space_read_4(sc->sc.iot, sc->nc_ioh, USBNC_USB_UH1_CTRL) |
		    (USBNC_USB_UH1_CTRL_OVER_CUR_POL | USBNC_USB_UH1_CTRL_OVER_CUR_DIS));
		break;
	}

	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_CLR,
	    USBPHY_CTRL_CLKGATE);

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	/* Stop then Reset */
	uint32_t val = EOREAD4(&sc->sc, EHCI_USBCMD);
	val &= ~EHCI_CMD_RS;
	EOWRITE4(&sc->sc, EHCI_USBCMD, val);

	while (EOREAD4(&sc->sc, EHCI_USBCMD) & EHCI_CMD_RS)
		;

	val = EOREAD4(&sc->sc, EHCI_USBCMD);
	val |= EHCI_CMD_HCRESET;
	EOWRITE4(&sc->sc, EHCI_USBCMD, val);

	while (EOREAD4(&sc->sc, EHCI_USBCMD) & EHCI_CMD_HCRESET)
		;

	/* Reset USBPHY module */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_SET, USBPHY_CTRL_SFTRST);

	delay(10);

	/* Remove CLKGATE and SFTRST */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_CLR,
	    USBPHY_CTRL_CLKGATE | USBPHY_CTRL_SFTRST);

	delay(10);

	/* Power up the PHY */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_PWD, 0);

	/* enable FS/LS device */
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, USBPHY_CTRL_SET,
	    USBPHY_CTRL_ENUTMILEVEL2 | USBPHY_CTRL_ENUTMILEVEL3);

	/* set host mode */
	EWRITE4(&sc->sc, EHCI_USBMODE,
	    EREAD4(&sc->sc, EHCI_USBMODE) | EHCI_USBMODE_HOST);

	/* set to UTMI mode */
	EOWRITE4(&sc->sc, EHCI_PORTSC(1),
	    EOREAD4(&sc->sc, EHCI_PORTSC(1)) & ~EHCI_PS_PTS_UTMI_MASK);

	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_USB,
	    ehci_intr, &sc->sc, devname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto mem3;
	}

	strlcpy(sc->sc.sc_vendor, "i.MX6", sizeof(sc->sc.sc_vendor));
	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);
		goto intr;
	}

	config_found(self, &sc->sc.sc_bus, usbctlprint);

	goto out;

intr:
	arm_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;
mem3:
	bus_space_unmap(sc->sc.iot, sc->nc_ioh, misc_reg[1]);
mem2:
	bus_space_unmap(sc->sc.iot, sc->ph_ioh, phy_reg[1]);
mem1:
mem0:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, faa->fa_reg[0].size);
	sc->sc.sc_size = 0;
out:
	return;
}

int
imxehci_detach(struct device *self, int flags)
{
	struct imxehci_softc		*sc = (struct imxehci_softc *)self;
	int				rv;

	rv = ehci_detach(self, flags);
	if (rv)
		return (rv);

	if (sc->sc_ih != NULL) {
		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

	return (0);
}

void
imxehci_enable_vbus(uint32_t phandle)
{
	uint32_t gpio[3];
	int active;
	int node;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return;

	if (!OF_is_compatible(node, "regulator-fixed"))
		return;

	imxiomuxc_pinctrlbyname(node, "default");

	if (OF_getproplen(node, "enable-active-high") == 0)
		active = 1;
	else
		active = 0;

	if (OF_getpropintarray(node, "gpio", gpio,
	    sizeof(gpio)) != sizeof(gpio))
		return;
	
	gpio_controller_config_pin(gpio, GPIO_CONFIG_OUTPUT);
	gpio_controller_set_pin(gpio, active);
}
